﻿#include "sessionupdater.h"
#include "datamodel.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRandomGenerator>
#include "settingmanager.h"
#include "proxymanager.h"

#define MAX_RETRY_COUNT 2

SessionUpdater::SessionUpdater(QObject *parent)
    : HttpThread{parent}
{

}


void SessionUpdater::run()
{
    qInfo("this is the thread of session updating");

    m_multiHandle = curl_multi_init();
    if (m_multiHandle == nullptr)
    {
        qCritical("failed to init a multi handle");
        return;
    }

    QVector<SessionUserData> userDatas;
    qint64 beginTime = GetTickCount64();
    while (!m_requestStop)
    {
        qint64 now = GetTickCount64();
        if (now - beginTime < 180000)
        {
            // 每3分钟更新一次
            QThread::msleep(100);
            continue;
        }
        beginTime = now;

        userDatas.clear();
        QVector<BuyParam> buyParams = m_buyParamManager->getBuyParams();
        for (int i=0; i<buyParams.size(); i++)
        {
            SessionUserData userData;
            userData.m_account = buyParams[i].m_user.m_accountName;
            userData.m_step = STEP_CHECK_EXPIRED;
            userDatas.append(userData);
        }

        for (int i=0; i<userDatas.size(); i++)
        {
            CURL* curl = makeSessionUpdateRequest(&userDatas[i]);
            if (curl == nullptr)
            {
                break;
            }
            curl_multi_add_handle(m_multiHandle, curl);
        }

        qInfo("begin to do the session updating, count is %d", userDatas.size());

        int stillRunning = 1;
        while (!m_requestStop && stillRunning)
        {
            int numfds = 0;
            curl_multi_wait(m_multiHandle, NULL, 0, 100, &numfds);


            CURLMcode mc = curl_multi_perform(m_multiHandle, &stillRunning);
            if (mc)
            {
                qCritical("curl_multi_perform return error: %d", mc);
                break;
            }

            while (true)
            {
                int msgs_left = 0;
                CURLMsg *m = curl_multi_info_read(m_multiHandle, &msgs_left);
                if (m == nullptr)
                {
                    break;
                }

                if (m->msg == CURLMSG_DONE && m->data.result == CURLE_OK)
                {
                    handleResponse(m->easy_handle);
                }
                else
                {
                    qCritical("failed to send request, error is %d", m->data.result);
                    retry(m->easy_handle);
                }

                curl_multi_remove_handle(m_multiHandle, m->easy_handle);
                freeRequest(m->easy_handle);

                // 有可能又添加了新请求
                stillRunning = true;
            }
        }

        // 释放所有请求
        CURL **list = curl_multi_get_handles(m_multiHandle);
        if (list)
        {
            for (int i = 0; list[i]; i++)
            {
                curl_multi_remove_handle(m_multiHandle, list[i]);
                freeRequest(list[i]);
            }
            curl_free(list);
        }

        qInfo("finish to do the session updating");

        // 统计报告下
        int successCount = 0;
        for (auto& userData : userDatas)
        {
            if (userData.m_success)
            {
                successCount++;
            }
        }

        QString logContent = QString::fromWCharArray(L"更新会话，总共%1, 成功%2, 失败%3")
                .arg(QString::number(userDatas.size()), QString::number(successCount),
                                     QString::number(userDatas.size()-successCount));
        emit printLog(logContent);
    }
    curl_multi_cleanup(m_multiHandle);
}

CURL* SessionUpdater::makeSessionUpdateRequest(SessionUserData* userData)
{
    ProxyServer proxyServer;
    if (SettingManager::getInstance()->m_useProxy)
    {
        proxyServer = ProxyManager::getInstance()->getProxyServer();
        if (proxyServer.m_ip.isEmpty())
        {
            qCritical("failed to get a proxy");
            return nullptr;
        }
    }

    QMap<QString, QString> headers;
    headers["origin"] = "https://www.apple.com/jp";
    headers["Referer"] = "https://www.apple.com/jp";

    BuyParam buyParam = m_buyParamManager->getBuyParam(userData->m_account);

    CURL* curl = nullptr;
    if (userData->m_step == STEP_CHECK_EXPIRED)
    {
        QString url = "https://www.apple.com/jp/shop/checkout";
        headers["Referer"] = "https://www.apple.com";
        curl = makeRequest(url, headers, buyParam.m_cookies, proxyServer);
    }
    else if (userData->m_step == STEP_EXTEND_SESSION)
    {
        QString url = buyParam.m_appStoreHost
                + QString("/shop/checkoutx/session?_a=extendSession&_m=checkout.session");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["Syntax"] = "graviton";
        headers["Modelversion"] = "v2";
        headers["X-Aos-Stk"] = buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment";
        curl = makeRequest(url, headers, buyParam.m_cookies, proxyServer);
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }

    if (curl == nullptr)
    {
        qCritical("failed to make request");
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_INTERFACE, buyParam.m_localIp.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_PRIVATE, userData);

    return curl;
}

void SessionUpdater::handleResponse(CURL* curl)
{
    SessionUserData* userData = nullptr;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &userData);
    if (userData == nullptr)
    {
        return;
    }

    long statusCode = 0;
    QString responseData;
    getResponse(curl, statusCode, responseData);

    // 网络请求报错，流程结束
    if (!(statusCode >= 200 && statusCode < 400))
    {
        qCritical("the response of updating session report error: %d", statusCode);
        retry(curl);
        return;
    }

    // 更新cookies
    QMap<QString, QString> cookies = getCookies(curl);
    m_buyParamManager->updateCookies(userData->m_account, cookies);

    if (userData->m_step == STEP_CHECK_EXPIRED)
    {
        if (statusCode == 200)
        {
            userData->m_retry = 0;
            userData->m_step = STEP_EXTEND_SESSION;
            CURL* nextCurl = makeSessionUpdateRequest(userData);
            if (nextCurl)
            {
                curl_multi_add_handle(m_multiHandle, nextCurl);
            }
        }
        else
        {
            if (statusCode == 302)
            {
                QString location = getLocationHeader(curl);
                if (location.indexOf("session_expired") > 0)
                {
                    if (!m_requestStop)
                    {
                        qInfo("session expired");
                        emit sessionExpired();
                        m_requestStop = true;
                    }
                    return;
                }
            }

            qCritical("the status of checking expired is %d", statusCode);
            retry(curl);
        }
    }
    else if (userData->m_step == STEP_EXTEND_SESSION)
    {
        if (statusCode == 200)
        {
            userData->m_success = true;
        }
        else
        {
            qCritical("the status of extending session is %d", statusCode);
            retry(curl);
        }
    }
}

void SessionUpdater::retry(CURL* curl)
{
    SessionUserData* userData = nullptr;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &userData);
    if (userData == nullptr)
    {
        return;
    }

    if (userData->m_retry < MAX_RETRY_COUNT)
    {
        qInfo("retry to update session, account is %s", userData->m_account.toStdString().c_str());
        userData->m_retry += 1;
        CURL* nextCurl = makeSessionUpdateRequest(userData);
        if (nextCurl)
        {
            curl_multi_add_handle(m_multiHandle, nextCurl);
        }
    }
}
