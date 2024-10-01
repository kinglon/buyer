﻿#include "goodsbuyer.h"
#include "datamodel.h"
#include <rsa.h>
#include <filters.h>
#include <osrng.h>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QFile>
#include <QTextStream>
#include "settingmanager.h"
#include "proxymanager.h"
#include "jsonutil.h"
#include "appledataparser.h"

#define APPLE_HOST "https://www.apple.com/jp"

// 重试请求间隔，2秒
#define RETRY_REQUEST_INTERVAL  2000

// 搜索店铺最大重试次数
#define MAX_SEARCH_SHOP_RETRY_COUNT  60

// 处理中最大重试次数
#define MAX_PROCESS_RETRY_COUNT  30

// 查询订单号最大重试次数
#define MAX_QUERY_ORDER_RETRY_COUNT  3

GoodsBuyer::GoodsBuyer(QObject *parent)
    : HttpThread{parent}
{

}

void GoodsBuyer::run()
{
    m_multiHandle = curl_multi_init();
    if (m_multiHandle == nullptr)
    {
        qCritical("failed to init a multi handle");
        emit buyFinish(this, nullptr);
    }

    // 初始化每个购买流程
    int64_t beginTime = GetTickCount64();
    QVector<BuyUserData*> buyUserDatas;
    for (int i=0; i<m_buyParams.size(); i++)
    {
        BuyUserData* userData = new BuyUserData();
        buyUserDatas.append(userData);
        userData->m_buyResult.m_account = m_buyParams[i].m_user.m_accountName;
        userData->m_buyResult.m_currentStep = STEP_SELECT_SHOP;
        userData->m_buyResult.m_localIp = m_buyParams[i].m_localIp;
        userData->m_buyParam = m_buyParams[i];
        QString takeTime = QString::fromWCharArray(L"初始化%1").arg(beginTime-userData->m_buyParam.m_beginBuyTime);
        userData->m_buyResult.m_takeTimes.append(takeTime);
        userData->m_buyResult.m_beginBuyDateTime = QDateTime::currentDateTime();
        if (!m_buyParams[i].m_proxyIp.isEmpty())
        {
            userData->m_buyResult.m_addCartProxy = m_buyParams[i].m_proxyIp+":"+QString::number(m_buyParams[i].m_proxyPort);
        }
        userData->m_buyResult.m_buyShopName = m_buyParams[i].m_buyingShop.m_name;
        userData->m_stepBeginTime = beginTime;

        CURL* curl = makeBuyingRequest(userData);
        if (curl == nullptr)
        {
            break;
        }
        curl_multi_add_handle(m_multiHandle, curl);
    }

    int64_t lastReportLogTime = GetTickCount64();
    bool needCheckGoodsAvailability = true;

    while (!m_requestStop)
    {        
        // 发送需要重传的请求
        qint64 now = GetTickCount64();
        while (!m_retryRequests.empty())
        {
            BuyUserData* userData = m_retryRequests.front();
            if (now - userData->m_lastRequestTime >= RETRY_REQUEST_INTERVAL)
            {
                m_retryRequests.pop_front();
                CURL* curl = makeBuyingRequest(userData);
                if (curl)
                {
                    curl_multi_add_handle(m_multiHandle, curl);
                }
                continue;
            }
            break;
        }

        int numfds = 0;
        curl_multi_wait(m_multiHandle, NULL, 0, 100, &numfds);

        int stillRunning = 0;
        CURLMcode mc = curl_multi_perform(m_multiHandle, &stillRunning);
        if (mc)
        {
            qCritical("curl_multi_perform return error: %d", mc);
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

            BuyUserData* userData = nullptr;
            curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &userData);
            if (userData)
            {
                userData->m_buyResult.m_failReason = QString::fromWCharArray(L"请求发送失败，错误码是%1").arg(m->data.result);
                QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                        .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
                emit printLog(log);
                m_buyResults.append(userData->m_buyResult);
            }
        }

        curl_multi_remove_handle(m_multiHandle, m->easy_handle);
        freeRequest(m->easy_handle);

        // 检查是否每个号都是无货
        if (needCheckGoodsAvailability)
        {
            bool allNotHave = true; // 所有号都没有货
            for (auto& userData : buyUserDatas)
            {
                if (userData->m_phoneAvailStatus == GoodsAvailStatus::HAVE)
                {
                    needCheckGoodsAvailability = false;
                    allNotHave = false;
                    break;
                }

                if (userData->m_phoneAvailStatus == GoodsAvailStatus::UNKNOWN)
                {
                    allNotHave = false;
                }
            }

            if (allNotHave)
            {
                emit printLog(QString::fromWCharArray(L"所有号都没手机"));
                break;
            }
        }

        if (m_buyResults.size() == m_buyParams.size())
        {
            break;
        }

        // 显示报告数据
        int elapse = GetTickCount64() - lastReportLogTime;
        if (elapse >= 10000)
        {
            QString stepRequestCountString;
            for (auto it=m_stepRequestCounts.begin(); it!=m_stepRequestCounts.end(); it++)
            {
                stepRequestCountString += ", step" + QString::number(it.key()) + "=" + QString::number(it.value());
            }
            QString logContent = QString::fromWCharArray(L"%1, 购买数=%2, 时长=%3")
                    .arg(m_name, QString::number(buyUserDatas.size()), QString::number(elapse));
            logContent += stepRequestCountString;
            emit printLog(logContent);

            lastReportLogTime = GetTickCount64();
            for (auto it=m_stepRequestCounts.begin(); it!=m_stepRequestCounts.end(); it++)
            {
                it.value() = 0;
            }
        }
    }

    // 释放所有请求
    CURL **list = curl_multi_get_handles(m_multiHandle);
    if (list)
    {
        for (int i = 0; list[i]; i++)
        {
            BuyUserData* userData = nullptr;
            curl_easy_getinfo(list[i], CURLINFO_PRIVATE, &userData);
            if (userData)
            {
                m_buyResults.append(userData->m_buyResult);
            }
            curl_multi_remove_handle(m_multiHandle, list[i]);
            freeRequest(list[i]);
        }
        curl_free(list);
    }
    curl_multi_cleanup(m_multiHandle);

    // 释放userdata
    for (auto& userData : buyUserDatas)
    {
        delete userData;
    }
    buyUserDatas.clear();

    emit buyFinish(this, new QVector<BuyResult>(m_buyResults));
}

QString GoodsBuyer::getBodyString(const QMap<QString, QString>& body)
{
    QString bodyStr;
    for (auto it=body.begin(); it!=body.end(); it++)
    {
        if (!bodyStr.isEmpty())
        {
            bodyStr += "&";
        }
        bodyStr += QUrl::toPercentEncoding(it.key()) + "=" + QUrl::toPercentEncoding(it.value());
    }
    return bodyStr;
}

CURL* GoodsBuyer::makeBuyingRequest(BuyUserData* userData)
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
    headers["origin"] = APPLE_HOST;
    headers["Referer"] = APPLE_HOST;
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";

    CURL* curl = nullptr;
    if (userData->m_buyResult.m_currentStep == STEP_SELECT_SHOP)
    {
        qInfo("[%s] select shop, id=%s, postal code=%s",
              userData->m_buyResult.m_account.toStdString().c_str(),
              userData->m_buyParam.m_buyingShop.m_storeNumber.toStdString().c_str(),
              userData->m_buyParam.m_buyingShop.m_postalCode.toStdString().c_str());

        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/fulfillment?_a=select&_m=checkout.fulfillment.pickupTab.pickup.storeLocator");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            QString body = QString("checkout.fulfillment.pickupTab.pickup.storeLocator.showAllStores=false&checkout.fulfillment.pickupTab.pickup.storeLocator.selectStore=%1&checkout.fulfillment.pickupTab.pickup.storeLocator.searchInput=%2")
                    .arg(userData->m_buyParam.m_buyingShop.m_storeNumber, userData->m_buyParam.m_buyingShop.m_postalCode);
            setPostMethod(curl, body);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_DATETIME)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/fulfillment?_a=select&_m=checkout.fulfillment.pickupTab.pickup.storeLocator");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            QString body = QString("checkout.fulfillment.pickupTab.pickup.storeLocator.showAllStores=false&checkout.fulfillment.pickupTab.pickup.storeLocator.selectStore=%1&checkout.fulfillment.pickupTab.pickup.storeLocator.searchInput=%2")
                    .arg(userData->m_buyParam.m_buyingShop.m_storeNumber, userData->m_buyParam.m_buyingShop.m_postalCode);
            setPostMethod(curl, body);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_SUBMIT_SHOP)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/fulfillment?_a=continueFromFulfillmentToPickupContact&_m=checkout.fulfillment");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            QMap<QString, QString> body;
            body["checkout.fulfillment.fulfillmentOptions.selectFulfillmentLocation"] = "RETAIL";
            body["checkout.fulfillment.pickupTab.pickup.storeLocator.showAllStores"] = "false";
            body["checkout.fulfillment.pickupTab.pickup.storeLocator.selectStore"] = userData->m_buyParam.m_buyingShop.m_storeNumber;
            body["checkout.fulfillment.pickupTab.pickup.storeLocator.searchInput"] = userData->m_buyParam.m_buyingShop.m_postalCode;
            if (!userData->m_date.isEmpty())
            {
                QJsonObject& timeObj = userData->m_time;
                if (timeObj.contains("checkInStart"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.startTime"]
                            = timeObj["checkInStart"].toString();
                }
                if (timeObj.contains("checkInEnd"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.endTime"]
                            = timeObj["checkInEnd"].toString();
                }
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.date"] = userData->m_date;
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.displayEndTime"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotType"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.isRecommended"] = "false";
                if (timeObj.contains("SlotId"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotId"]
                            = timeObj["SlotId"].toString();
                }
                if (timeObj.contains("signKey"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.signKey"]
                            = timeObj["signKey"].toString();
                }
                if (timeObj.contains("timeZone"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeZone"]
                            = timeObj["timeZone"].toString();
                }
                if (timeObj.contains("timeSlotValue"))
                {
                    body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotValue"]
                            = timeObj["timeSlotValue"].toString();
                }
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.isRestricted"] = "false";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.displayStartTime"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.displayStartTime"] = "";
            }
            QString bodyData = getBodyString(body);
            setPostMethod(curl, bodyData);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_REVIEW)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/review?_a=continueFromReviewToProcess&_m=checkout.review.placeOrder");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Review";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PROCESS)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx?_a=pollingProcess&_m=spinner");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Process";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }    
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkout/thankyou");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Process";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, proxyServer);
    }

    if (curl == nullptr)
    {
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_INTERFACE, userData->m_buyParam.m_localIp.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_PRIVATE, userData);

    userData->m_lastRequestTime = GetTickCount64();
    m_stepRequestCounts[userData->m_buyResult.m_currentStep] += 1;

    qInfo("[%s] begin to send request %d", userData->m_buyResult.m_account.toStdString().c_str(),
          userData->m_buyResult.m_currentStep);

    return curl;
}

void GoodsBuyer::handleResponse(CURL* curl)
{
    BuyUserData* userData = nullptr;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &userData);
    if (userData == nullptr)
    {
        return;
    }

    qInfo("[%s] receive the response of request %d", userData->m_buyResult.m_account.toStdString().c_str(),
          userData->m_buyResult.m_currentStep);

    if (userData->m_buyResult.m_currentStep <= STEP_REVIEW)
    {
        int64_t now = GetTickCount64();
        int takeTime = int(now - userData->m_stepBeginTime);
        QString takeTimeString = QString::fromWCharArray(L"%1%2").arg(userData->m_buyResult.getStepName(), QString::number(takeTime));
        userData->m_buyResult.m_takeTimes.append(takeTimeString);
        userData->m_stepBeginTime = now;
    }

    long statusCode = 0;
    QString responseData;
    getResponse(curl, statusCode, responseData);

    // 网络请求报错，流程结束
    if (!(statusCode >= 200 && statusCode < 400))
    {
        userData->m_buyResult.m_failReason = QString::fromWCharArray(L"服务器返回%1").arg(statusCode);
        QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
        emit printLog(log);
        m_buyResults.append(userData->m_buyResult);
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

    if (userData->m_buyResult.m_currentStep == STEP_SELECT_SHOP)
    {
        if (userData->m_buyParam.m_enableDebug)
        {
            saveDataToFile(responseData, "STEP_SEARCH_SHOP.txt");
        }

        bool pickup = false;
        if (!AppleDataParser::checkIfPickup(responseData, pickup))
        {
            retryRequest(userData);
        }
        else
        {
            if (userData->m_buyParam.m_enableDebug)
            {
                if (pickup)
                {
                    emit printLog(QString::fromWCharArray(L"可自提"));
                }
                else
                {
                    emit printLog(QString::fromWCharArray(L"不可自提"));
                }
            }

            if (pickup)
            {
                userData->m_phoneAvailStatus = GoodsAvailStatus::HAVE;
                // 有手机就让它走下去
                enterStep(userData, STEP_SUBMIT_SHOP);
            }
            else
            {                
                userData->m_phoneAvailStatus = GoodsAvailStatus::NOT_HAVE;
                if (userData->m_retryCount < MAX_SEARCH_SHOP_RETRY_COUNT)
                {
                    retryRequest(userData);
                }
                else
                {
                    userData->m_buyResult.m_failReason = QString::fromWCharArray(L"不可自提");
                    QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                            .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
                    emit printLog(log);
                    m_buyResults.append(userData->m_buyResult);
                }
            }
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_DATETIME)
    {
        if (userData->m_buyParam.m_enableDebug)
        {
            saveDataToFile(responseData, "STEP_QUERY_DATETIME.txt");
        }

        handleQueryDateTimeResponse(userData, responseData);

        QString checkInStart;
        QString checkInEnd;
        if (userData->m_time.contains("checkInStart"))
        {
            checkInStart = userData->m_time["checkInStart"].toString();
        }
        if (userData->m_time.contains("checkInEnd"))
        {
            checkInEnd = userData->m_time["checkInEnd"].toString();
        }
        qInfo("[%s] query datetime result, date=%s, time start=%s, time end=%s",
              userData->m_buyResult.m_account.toStdString().c_str(),
              userData->m_date.toStdString().c_str(),
              checkInStart.toStdString().c_str(),
              checkInEnd.toStdString().c_str());

        enterStep(userData, STEP_SELECT_SHOP);
    }
    else if (userData->m_buyResult.m_currentStep == STEP_SUBMIT_SHOP)
    {
        if (userData->m_buyParam.m_enableDebug)
        {
            saveDataToFile(responseData, "STEP_SELECT_SHOP.txt");
        }

        bool pickup = false;
        if (!AppleDataParser::checkIfPickup(responseData, pickup))
        {
            qCritical("failed to check if pickup, continue to review step");
            enterStep(userData, STEP_REVIEW);
        }
        else
        {
            if (pickup)
            {
                enterStep(userData, STEP_REVIEW);
            }
            else
            {
                userData->m_buyResult.m_failReason = QString::fromWCharArray(L"不可自提");
                QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                        .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
                emit printLog(log);
                m_buyResults.append(userData->m_buyResult);
            }
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_REVIEW)
    {
        if (userData->m_buyParam.m_enableDebug)
        {
            saveDataToFile(responseData, "STEP_REVIEW.txt");
        }

        qint64 totalTime = GetTickCount64() - userData->m_buyParam.m_beginBuyTime;
        QString takeTime = QString::fromWCharArray(L"总%1").arg(totalTime);
        userData->m_buyResult.m_takeTimes.append(takeTime);
        enterStep(userData, STEP_PROCESS);
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PROCESS)
    {
        if (userData->m_buyParam.m_enableDebug)
        {
            saveDataToFile(responseData, "STEP_PROCESS.txt");
        }

        if (!handleProcessResponse(userData, responseData))
        {
            if (userData->m_retryCount < MAX_PROCESS_RETRY_COUNT)
            {
                retryRequest(userData);
            }
            else
            {
                userData->m_buyResult.m_failReason = QString::fromWCharArray(L"等待处理超时");
                QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                        .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
                emit printLog(log);
                m_buyResults.append(userData->m_buyResult);
            }
        }
        else
        {
            if (userData->m_buyResult.m_success)
            {
                enterStep(userData, STEP_QUERY_ORDER_NO);
            }
            else
            {
                m_buyResults.append(userData->m_buyResult);
            }
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        if (handleQueryOrderResponse(userData, responseData))
        {
            m_buyResults.append(userData->m_buyResult);
        }
        else
        {
            if (userData->m_retryCount < MAX_QUERY_ORDER_RETRY_COUNT)
            {
                retryRequest(userData);
            }
            else
            {
                m_buyResults.append(userData->m_buyResult);
            }
        }
    }
}

void GoodsBuyer::handleQueryDateTimeResponse(BuyUserData* userData, QString& responseData)
{
    QByteArray jsonData = responseData.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse the query date time response data");
        return;
    }

    QJsonObject root = jsonDocument.object();
    if (!root.contains("body"))
    {
        return;
    }
    QJsonObject bodyJson = root["body"].toObject();

    if (!bodyJson.contains("checkout"))
    {
        return;
    }
    QJsonObject checkoutJson = bodyJson["checkout"].toObject();

    if (!checkoutJson.contains("fulfillment"))
    {
        return;
    }
    QJsonObject fulfillmentJson = checkoutJson["fulfillment"].toObject();

    if (!fulfillmentJson.contains("pickupTab"))
    {
        return;
    }
    QJsonObject pickupTabJson = fulfillmentJson["pickupTab"].toObject();

    if (!pickupTabJson.contains("pickup"))
    {
        return;
    }
    QJsonObject pickupJson = pickupTabJson["pickup"].toObject();

    if (!pickupJson.contains("timeSlot"))
    {
        qInfo("not need to select datetime");
        return;
    }
    QJsonObject timeSlotJson = pickupJson["timeSlot"].toObject();

    if (!timeSlotJson.contains("dateTimeSlots"))
    {
        return;
    }
    QJsonObject dateTimeSlotsJson = timeSlotJson["dateTimeSlots"].toObject();

    if (!dateTimeSlotsJson.contains("d"))
    {
        return;
    }
    QJsonObject dJson = dateTimeSlotsJson["d"].toObject();

    if (!dJson.contains("pickUpDates"))
    {
        return;
    }
    QJsonArray pickUpDatesJson = dJson["pickUpDates"].toArray();
    if (pickUpDatesJson.size() == 0)
    {
        qCritical("not have any date to buy");
        return;
    }

    int randomNumber = QRandomGenerator::global()->bounded(pickUpDatesJson.size());
    QJsonObject pickUpDateJson = pickUpDatesJson[randomNumber].toObject();
    if (pickUpDateJson.contains("date"))
    {
        userData->m_date = pickUpDateJson["date"].toString();
    }

    if (!pickUpDateJson.contains("dayOfMonth"))
    {
        return;
    }
    QString day = pickUpDateJson["dayOfMonth"].toString();

    if (!dJson.contains("timeSlotWindows"))
    {
        return;
    }
    QJsonArray timeSlotWindows = dJson["timeSlotWindows"].toArray();
    for (auto timeSlotWindow : timeSlotWindows)
    {
        QJsonObject timeSlotWindowJson = timeSlotWindow.toObject();
        if (timeSlotWindowJson.contains(day))
        {
            QJsonArray timeSlotsJson = timeSlotWindowJson[day].toArray();
            if (timeSlotsJson.size() == 0)
            {
                return;
            }

            randomNumber = QRandomGenerator::global()->bounded(timeSlotsJson.size());
            userData->m_time = timeSlotsJson[randomNumber].toObject();
            break;
        }
    }
}

bool GoodsBuyer::getGoodsAvalibility(BuyUserData* userData, QString& responseData, bool& hasPhone, bool& hasRecommend)
{
    QByteArray jsonData = responseData.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse the search shop response data");
        return false;
    }

    QJsonObject root = jsonDocument.object();
    QVector<QString> keys;
    keys.append("body");
    keys.append("checkout");
    keys.append("fulfillment");
    keys.append("pickupTab");
    keys.append("pickup");
    keys.append("storeLocator");
    keys.append("searchResults");
    keys.append("d");
    QJsonObject dJson;
    if (!JsonUtil::findObject(root, keys, dJson) || !dJson.contains("retailStores"))
    {
        qCritical("failed to find the retailStores node");
        return false;
    }

    QJsonArray retailStoresJson = dJson["retailStores"].toArray();
    for (auto retailStoreItem : retailStoresJson)
    {
        QJsonObject retailStoreJson = retailStoreItem.toObject();
        QString storeId = retailStoreJson["storeId"].toString();
        if (storeId == userData->m_buyParam.m_buyingShop.m_storeNumber)
        {
            hasPhone = false;
            hasRecommend = false;
            QJsonArray lineItemAvailabilityJson = retailStoreJson["availability"].toObject()["lineItemAvailability"].toArray();
            for (auto lineItem : lineItemAvailabilityJson)
            {
                QJsonObject lineItemJson = lineItem.toObject();
                if (lineItemJson["partName"].toString().indexOf("iPhone") >= 0)
                {
                    hasPhone = lineItemJson["availableNowForLine"].toBool();
                }
                else
                {
                    hasRecommend = lineItemJson["availableNowForLine"].toBool();
                }
            }
            return true;
        }
    }

    qCritical("failed to find the desired shop: %s", userData->m_buyParam.m_buyingShop.m_storeNumber.toStdString().c_str());
    return false;
}

bool GoodsBuyer::handleProcessResponse(BuyUserData* userData, QString& responseData)
{
    if (responseData.indexOf("thankyou") >= 0)
    {
        userData->m_buyResult.m_success = true;
        QString log = QString::fromWCharArray(L"账号(%1)下单成功")
                .arg(userData->m_buyResult.m_account);
        emit printLog(log);
        return true;
    }

    QByteArray jsonData = responseData.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse the process response data");
        return false;
    }

    QJsonObject root = jsonDocument.object();
    if (responseData.indexOf("sorry") >= 0 &&
            root.contains("head") && root["head"].toObject().contains("data") &&
            root["head"].toObject()["data"].toObject().contains("url"))
    {
        QString sorryUrl = root["head"].toObject()["data"].toObject()["url"].toString();
        userData->m_buyResult.m_failReason = sorryUrl;
        QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                .arg(userData->m_buyResult.m_account, sorryUrl);
        emit printLog(log);
        return true;
    }

    if (root.contains("body") && root["body"].toObject().contains("meta") &&
            root["body"].toObject()["meta"].toObject().contains("page") &&
            root["body"].toObject()["meta"].toObject()["page"].toObject().contains("title"))
    {
        QString title = root["body"].toObject()["meta"].toObject()["page"].toObject()["title"].toString();
        userData->m_buyResult.m_failReason = title;
        QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                .arg(userData->m_buyResult.m_account, title);
        emit printLog(log);
        return true;
    }

    // 判断是不是没货
    bool hasPhone = false;
    bool hasRecommend = false;
    if (getGoodsAvalibility(userData, responseData, hasPhone, hasRecommend))
    {
        if (!hasPhone || !hasRecommend)
        {
            userData->m_buyResult.m_failReason = AppleDataParser::getGoodsAvailabilityString(hasPhone, hasRecommend);
            QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
                    .arg(userData->m_buyResult.m_account, userData->m_buyResult.m_failReason);
            emit printLog(log);
            return true;
        }
    }

    return false;
}

bool GoodsBuyer::handleQueryOrderResponse(BuyUserData* userData, QString& data)
{
    int begin = data.indexOf("\"orderNumber\":");
    if (begin > 0)
    {
        begin = data.indexOf('"', begin + strlen("\"orderNumber\":"));
        if (begin > 0)
        {
            int end = data.indexOf('"', begin + 1);
            QString orderNumber = data.mid(begin + 1, end - begin -1);
            if (!orderNumber.isEmpty())
            {
                userData->m_buyResult.m_orderNo = orderNumber;
                QString log = QString::fromWCharArray(L"账号(%1)订单号是%2")
                        .arg(userData->m_buyResult.m_account, orderNumber);
                emit printLog(log);
                return true;
            }
        }
    }

    return false;
}

void GoodsBuyer::getCreditCardInfo(QString cardNo, QString& cardNumberPrefix, QString& cardNoCipher)
{
    if (cardNo.size() < 7)
    {
        cardNumberPrefix = cardNo;
    }
    else
    {
        cardNumberPrefix = cardNo.left(4) + " " + cardNo.mid(4, 2);
    }

    QString publicKey = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvUIrYPRsCjQNCEGNWmSp9Wz+5uSqK6nkwiBq254Q5taDOqZz0YGL3s1DnJPuBU+e8Dexm6GKW1kWxptTRtva5Eds8VhlAgph8RqIoKmOpb3uJOhSzBpkU28uWyi87VIMM2laXTsSGTpGjSdYjCbcYvMtFdvAycfuEuNn05bDZvUQEa+j9t4S0b2iH7/8LxLos/8qMomJfwuPwVRkE5s5G55FeBQDt/KQIEDvlg1N8omoAjKdfWtmOCK64XZANTG2TMnar/iXyegPwj05m443AYz8x5Uw/rHBqnpiQ4xg97Ewox+SidebmxGowKfQT3+McmnLYu/JURNlYYRy2lYiMwIDAQAB";
    QByteArray publicKeyBytes = QByteArray::fromBase64(publicKey.toUtf8());
    CryptoPP::ArraySource publicKeySource((const byte*)publicKeyBytes.data(), publicKeyBytes.length(), true);
    CryptoPP::RSA::PublicKey rsaPublicKey;
    rsaPublicKey.Load(publicKeySource);

    CryptoPP::AutoSeededRandomPool rng;
    CryptoPP::RSAES< CryptoPP::OAEP<CryptoPP::SHA256> >::Encryptor encryptor(rsaPublicKey);
    std::string ciphertext;
    CryptoPP::StringSource(cardNo.toUtf8(), true,
        new CryptoPP::PK_EncryptorFilter(rng, encryptor,
            new CryptoPP::StringSink(ciphertext)
        )
    );
    QByteArray base64ByteArray = QByteArray(ciphertext.c_str(), ciphertext.length()).toBase64();
    QString base64CipherText = QString::fromUtf8(base64ByteArray);
    cardNoCipher = QString("{\"cipherText\":\"%1\",\"publicKeyHash\":\"DsCuZg+6iOaJUKt5gJMdb6rYEz9BgEsdtEXjVc77oAs=\"}")
            .arg(base64CipherText);
}

void GoodsBuyer::retryRequest(BuyUserData* userData)
{
    m_retryRequests.push_back(userData);
    userData->m_retryCount += 1;
}

void GoodsBuyer::enterStep(BuyUserData* userData, int step)
{
    userData->m_buyResult.m_currentStep = step;
    userData->m_retryCount = 0;
    CURL* nextCurl = makeBuyingRequest(userData);
    if (nextCurl)
    {
        curl_multi_add_handle(m_multiHandle, nextCurl);
    }
}

void GoodsBuyer::saveDataToFile(const QString& data, QString fileName)
{
    qInfo("save data to file: %s", fileName.toStdString().c_str());

    QFile file(m_planDataPath + "\\" + fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream out(&file);
    out << data;

    file.close();
}
