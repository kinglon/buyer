#include "goodsbuyer.h"
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

// 选择店铺最大重试次数
#define MAX_SELECT_SHOP_RETRY_COUNT  60

// 处理中最大重试次数
#define MAX_PROCESS_RETRY_COUNT  30

// 查询订单号最大重试次数
#define MAX_QUERY_ORDER_RETRY_COUNT  3

// 服务器报错(如返回503)最大重试次数
#define MAX_SERVER_ERROR_RETRY_COUNT 2

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
                emit printLog(QString::fromWCharArray(L"%1, 所有号都没手机").arg(m_name));
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
            if (!userData->m_pickupDateTime.m_date.isEmpty())
            {
                QJsonObject& timeObj = userData->m_pickupDateTime.m_time;
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.startTime"] = timeObj["checkInStart"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.endTime"] = timeObj["checkInEnd"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.date"] = userData->m_pickupDateTime.m_date;
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.displayEndTime"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotType"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.isRecommended"] = "false";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotId"] = timeObj["SlotId"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.signKey"] = timeObj["signKey"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeZone"] = timeObj["timeZone"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.timeSlotValue"] = timeObj["timeSlotValue"].toString();
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.isRestricted"] = "false";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.displayStartTime"] = "";
                body["checkout.fulfillment.pickupTab.pickup.timeSlot.dateTimeSlots.dayRadio"] = userData->m_pickupDateTime.m_dayRadio;
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
        if (userData->m_retryCount < MAX_SERVER_ERROR_RETRY_COUNT)
        {
            retryRequest(userData);
        }
        else
        {
            QString error = QString::fromWCharArray(L"服务器返回%1").arg(statusCode);
            finishBuyWithError(userData, error);
        }

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
        handleSelectShopResponse(userData, responseData);
    }    
    else if (userData->m_buyResult.m_currentStep == STEP_SUBMIT_SHOP)
    {
        handleSubmitShopResponse(userData, responseData);
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
                finishBuyWithError(userData, QString::fromWCharArray(L"等待处理超时"));
            }
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        if (!handleQueryOrderResponse(userData, responseData))
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

void GoodsBuyer::handleSelectShopResponse(BuyUserData* userData, QString& responseData)
{
    if (userData->m_buyParam.m_enableDebug)
    {
        saveDataToFile(responseData, "STEP_SELECT_SHOP.txt");
    }

    // 检查是否有货
    bool hasPhone = false;
    bool hasRecommend = false;
    if (!getGoodsAvalibility(userData, responseData, hasPhone, hasRecommend))
    {
        if (userData->m_retryCount < MAX_SELECT_SHOP_RETRY_COUNT)
        {
            retryRequest(userData);
        }
        else
        {
            finishBuyWithError(userData, QString::fromWCharArray(L"解析是否有货失败"));
        }
        return;
    }

    // 检查是否自提
    bool pickup = false;
    if (!AppleDataParser::checkIfPickup(responseData, pickup))
    {
        if (userData->m_retryCount < MAX_SELECT_SHOP_RETRY_COUNT)
        {
            retryRequest(userData);
        }
        else
        {
            finishBuyWithError(userData, QString::fromWCharArray(L"解析是否自提失败"));
        }
        return;
    }

    // 显示查询结果
    if (userData->m_buyParam.m_enableDebug)
    {
        QString log;
        if (pickup)
        {
            log += QString::fromWCharArray(L"可以自提");
        }
        else
        {
            log += QString::fromWCharArray(L"不可自提");
        }

        if (hasPhone)
        {
            log += QString::fromWCharArray(L", 有手机");
        }
        else
        {
            log += QString::fromWCharArray(L", 无手机");
        }

        if (hasRecommend)
        {
            log += QString::fromWCharArray(L", 有配件");
        }
        else
        {
            log += QString::fromWCharArray(L", 无配件");
        }

        emit printLog(log);
    }

    // 不可自提或无货，直接结束
    if (!pickup || !hasPhone || !hasRecommend)
    {
        userData->m_phoneAvailStatus = GoodsAvailStatus::NOT_HAVE;
        if (userData->m_retryCount < MAX_SELECT_SHOP_RETRY_COUNT)
        {
            retryRequest(userData);
        }
        else
        {
            finishBuyWithError(userData, QString::fromWCharArray(L"不可自提或无货"));
        }
        return;
    }
    userData->m_phoneAvailStatus = GoodsAvailStatus::HAVE;

    // 查询自取日期和时间
    PickupDateTime pickupDateTime;
    if (!AppleDataParser::getPickupDateTime(responseData, pickupDateTime))
    {
        if (userData->m_retryCount < MAX_SELECT_SHOP_RETRY_COUNT)
        {
            retryRequest(userData);
        }
        else
        {
            finishBuyWithError(userData, QString::fromWCharArray(L"获取自提日期时间失败"));
        }
        return;
    }
    qInfo("[%s] query datetime result, date=%s, time start=%s, time end=%s",
          userData->m_buyResult.m_account.toStdString().c_str(),
          pickupDateTime.m_date.toStdString().c_str(),
          pickupDateTime.m_time["checkInStart"].toString().toStdString().c_str(),
          pickupDateTime.m_time["checkInEnd"].toString().toStdString().c_str());
    userData->m_pickupDateTime = pickupDateTime;

    // 继续下一步
    enterStep(userData, STEP_SUBMIT_SHOP);
}

void GoodsBuyer::handleSubmitShopResponse(BuyUserData* userData, QString& responseData)
{
    if (userData->m_buyParam.m_enableDebug)
    {
        saveDataToFile(responseData, "STEP_SUBMIT_SHOP.txt");
    }

    // 检查是否有货
    bool hasPhone = false;
    bool hasRecommend = false;
    if (!getGoodsAvalibility(userData, responseData, hasPhone, hasRecommend))
    {
        finishBuyWithError(userData, QString::fromWCharArray(L"解析是否有货失败"));
        return;
    }

    // 检查是否自提
    bool pickup = false;
    if (!AppleDataParser::checkIfPickup(responseData, pickup))
    {
        finishBuyWithError(userData, QString::fromWCharArray(L"解析是否自提失败"));
        return;
    }

    // 不可自提或无货，直接结束
    if (!pickup || !hasPhone || !hasRecommend)
    {
        finishBuyWithError(userData, QString::fromWCharArray(L"不可自提或无货"));
        return;
    }

    // 继续下一步
    enterStep(userData, STEP_REVIEW);
}

bool GoodsBuyer::getGoodsAvalibility(BuyUserData* userData, QString& responseData, bool& hasPhone, bool& hasRecommend)
{
    QVector<GoodsDetail> goodsDetails = AppleDataParser::parseGoodsDetail(responseData);
    if (goodsDetails.size() == 0)
    {
        return false;
    }

    for (auto& goodDetail : goodsDetails)
    {
        if (goodDetail.m_strStoreId == userData->m_buyParam.m_buyingShop.m_storeNumber)
        {
            hasPhone = goodDetail.m_hasPhone;
            hasRecommend = goodDetail.m_hasRecommend;
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
        enterStep(userData, STEP_QUERY_ORDER_NO);
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
        finishBuyWithError(userData, sorryUrl);
        return true;
    }

    if (root.contains("body") && root["body"].toObject().contains("meta") &&
            root["body"].toObject()["meta"].toObject().contains("page") &&
            root["body"].toObject()["meta"].toObject()["page"].toObject().contains("title"))
    {
        QString title = root["body"].toObject()["meta"].toObject()["page"].toObject()["title"].toString();
        finishBuyWithError(userData, title);
        return true;
    }

    // 判断是不是没货
    bool hasPhone = false;
    bool hasRecommend = false;
    if (getGoodsAvalibility(userData, responseData, hasPhone, hasRecommend))
    {
        if (!hasPhone || !hasRecommend)
        {
            QString error = AppleDataParser::getGoodsAvailabilityString(hasPhone, hasRecommend);
            finishBuyWithError(userData, error);
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
                m_buyResults.append(userData->m_buyResult);
                return true;
            }
        }
    }

    return false;
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

void GoodsBuyer::finishBuyWithError(BuyUserData* userData, QString error)
{
    userData->m_buyResult.m_failReason = error;
    QString log = QString::fromWCharArray(L"账号(%1)下单失败：%2")
            .arg(userData->m_buyResult.m_account, error);
    emit printLog(log);
    m_buyResults.append(userData->m_buyResult);
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
