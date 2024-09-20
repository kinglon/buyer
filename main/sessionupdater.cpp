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

SessionUpdater::SessionUpdater(QObject *parent)
    : HttpThread{parent}
{

}

QVector<BuyParam>& SessionUpdater::getBuyParams()
{
    m_mutex.lock();
    return m_buyParams;
    m_mutex.unlock();
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

        m_mutex.lock();
        userDatas.clear();
        for (int i=0; i<m_buyParams.size(); i++)
        {
            SessionUserData userData;
            userData.m_buyParam = m_buyParams[i];
            userData.m_step = STEP_SESSION1;
            userDatas.append(userData);
        }
        m_mutex.unlock();

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

        while (!m_requestStop)
        {
            int numfds = 0;
            curl_multi_wait(m_multiHandle, NULL, 0, 100, &numfds);

            int stillRunning = 0;
            CURLMcode mc = curl_multi_perform(m_multiHandle, &stillRunning);
            if (mc)
            {
                qCritical("curl_multi_perform return error: %d", mc);
                break;
            }

            if (stillRunning == 0)
            {
                break;
            }

            int msgs_left = 0;
            CURLMsg *m = curl_multi_info_read(m_multiHandle, &msgs_left);
            if (m == nullptr)
            {
                continue;
            }

            if (m->msg == CURLMSG_DONE && m->data.result == CURLE_OK)
            {
                handleResponse(m->easy_handle);
            }
            else
            {
                qCritical("failed to send request, error is %d", m->data.result);
            }

            curl_multi_remove_handle(m_multiHandle, m->easy_handle);
            freeRequest(m->easy_handle);
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
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";

    CURL* curl = nullptr;
    if (userData->m_step == STEP_SESSION1)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/session?_a=extendSessionUrl&_m=checkout.session");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }
    else if (userData->m_step == STEP_SESSION2)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/session?_a=extendSession&_m=checkout.session");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }

    if (curl == nullptr)
    {
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_INTERFACE, userData->m_buyParam.m_localIp.toStdString().c_str());
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
        return;
    }

    // 更新cookies
    QMap<QString, QString> cookies = getCookies(curl);
    for (auto it=cookies.begin(); it!=cookies.end(); it++)
    {
        if (it.value() == "DELETED")
        {
            userData->m_buyParam.m_cookies.remove(it.key());
        }
        else
        {
            userData->m_buyParam.m_cookies[it.key()] = it.value();
        }
    }

    m_mutex.lock();
    for (auto& buyParam : m_buyParams)
    {
        if (buyParam.m_user.m_accountName == userData->m_buyParam.m_user.m_accountName)
        {
            buyParam.m_cookies = userData->m_buyParam.m_cookies;
            break;
        }
    }
    m_mutex.unlock();

    if (userData->m_step == STEP_SESSION1)
    {
        userData->m_step = STEP_SESSION2;
        CURL* nextCurl = makeSessionUpdateRequest(userData);
        if (nextCurl)
        {
            curl_multi_add_handle(m_multiHandle, nextCurl);
        }
    }
}
