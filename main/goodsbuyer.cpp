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
#include "settingmanager.h"

#define APPLE_HOST "https://www.apple.com/jp"

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

    // 初始化每个购买流程，从选择店铺步骤开始
    int64_t beginTime = GetTickCount64();
    for (int i=0; i<m_buyParams.size(); i++)
    {
        BuyUserData* userData = new BuyUserData();
        userData->m_buyResult.m_account = m_buyParams[i].m_user.m_accountName;
        userData->m_buyResult.m_currentStep = STEP_FULFILLMENT_STORE;
        userData->m_buyResult.m_localIp = m_localIps[i%m_localIps.size()];
        userData->m_buyParam = m_buyParams[i];
        userData->m_buyResult.m_takeTimes.append(GetTickCount64()-userData->m_buyParam.m_beginBuyTime);
        userData->m_stepBeginTime = beginTime;

        CURL* curl = makeBuyingRequest(userData);
        if (curl == nullptr)
        {
            break;
        }
        curl_multi_add_handle(m_multiHandle, curl);
    }

    while (!m_requestStop)
    {        
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
            BuyUserData* userData = nullptr;
            curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &userData);
            if (userData)
            {
                m_buyResults.append(userData->m_buyResult);
                delete userData;
            }
        }

        curl_multi_remove_handle(m_multiHandle, m->easy_handle);
        freeRequest(m->easy_handle);

        if (m_buyResults.size() == m_buyParams.size())
        {
            break;
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
                delete userData;
            }
            curl_multi_remove_handle(m_multiHandle, list[i]);
            freeRequest(list[i]);
        }
        curl_free(list);
    }
    curl_multi_cleanup(m_multiHandle);

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
    QMap<QString, QString> headers;
    headers["origin"] = APPLE_HOST;
    headers["Referer"] = APPLE_HOST;
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";

    CURL* curl = nullptr;
    if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_STORE)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/fulfillment?_a=continueFromFulfillmentToPickupContact&_m=checkout.fulfillment");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Fulfillment-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            QString body = QString("checkout.fulfillment.fulfillmentOptions.selectFulfillmentLocation=RETAIL&checkout.fulfillment.pickupTab.pickup.storeLocator.showAllStores=false&checkout.fulfillment.pickupTab.pickup.storeLocator.selectStore=%1&checkout.fulfillment.pickupTab.pickup.storeLocator.searchInput=%2")
                    .arg(userData->m_buyParam.m_buyingShop.m_storeNumber, userData->m_buyParam.m_buyingShop.m_postalCode);
            setPostMethod(curl, body);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PICKUP_CONTACT)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx?_a=continueFromPickupContactToBilling&_m=checkout.pickupContact");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=PickupContact-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            QMap<QString, QString> body;
            body["checkout.pickupContact.pickupContactOptions.selectedPickupOption"] = "SELF";
            body["checkout.pickupContact.selfPickupContact.selfContact.address.emailAddress"] = userData->m_buyParam.m_user.m_email;
            body["checkout.pickupContact.selfPickupContact.selfContact.address.lastName"] = userData->m_buyParam.m_user.m_lastName;
            body["checkout.pickupContact.selfPickupContact.selfContact.address.firstName"] = userData->m_buyParam.m_user.m_firstName;
            body["checkout.pickupContact.selfPickupContact.selfContact.address.mobilePhone"] = userData->m_buyParam.m_user.m_telephone;
            body["checkout.pickupContact.selfPickupContact.selfContact.address.isDaytimePhoneSelected"] = "false";

            QString bodyData = getBodyString(body);
            setPostMethod(curl, bodyData);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_BILLING)
    {
        QString url = userData->m_buyParam.m_appStoreHost
                + QString("/shop/checkoutx/billing?_a=continueFromBillingToReview&_m=checkout.billing");
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;
        headers["X-Requested-With"] = "Fetch";
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = userData->m_buyParam.m_appStoreHost
                + "/shop/checkout?_s=Billing-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            QString creditCardPrefix;
            QString creditCardCipher;
            getCreditCardInfo(userData->m_buyParam.m_user.m_creditCardNo, creditCardPrefix, creditCardCipher);

            QMap<QString, QString> body;
            body["checkout.billing.billingOptions.selectBillingOption"] = "CREDIT";
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.validCardNumber"] = "true";
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumberForBinDetection"] = creditCardPrefix;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.selectCardType"] = "MASTERCARD";
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.securityCode"] = userData->m_buyParam.m_user.m_cvv;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.expiration"] = userData->m_buyParam.m_user.m_expiredData;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumber"] = creditCardCipher;
            body["checkout.locationConsent.locationConsent"] = "false";
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.city"] = userData->m_buyParam.m_user.m_city;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.state"] = userData->m_buyParam.m_user.m_state;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.lastName"] = userData->m_buyParam.m_user.m_lastName;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.firstName"] = userData->m_buyParam.m_user.m_firstName;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.countryCode"] = "JP";
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.street"] = userData->m_buyParam.m_user.m_street;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.postalCode"] = userData->m_buyParam.m_user.m_postalCode;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.street2"] = userData->m_buyParam.m_user.m_street2;
            body["checkout.billing.billingOptions.selectedBillingOptions.creditCard.creditInstallment.selectedInstallment"] = "1";
            body["checkout.billing.billingOptions.selectedBillingOptions.giftCard.giftCardInput.deviceID"] = "TF1;015;;;;;;;;;;;;;;;;;;;;;;Mozilla;Netscape;5.0%20%28Windows%20NT%2010.0%3B%20Win64%3B%20x64%29%20AppleWebKit/537.36%20%28KHTML%2C%20like%20Gecko%29%20Chrome/122.0.0.0%20Safari/537.36;20030107;undefined;true;;true;Win32;undefined;Mozilla/5.0%20%28Windows%20NT%2010.0%3B%20Win64%3B%20x64%29%20AppleWebKit/537.36%20%28KHTML%2C%20like%20Gecko%29%20Chrome/122.0.0.0%20Safari/537.36;zh-CN;undefined;secure6.store.apple.com;undefined;undefined;undefined;undefined;false;false;1719667606222;8;2005/6/7%2021%3A33%3A44;1366;768;;;;;;;;-480;-480;2024/6/29%2021%3A26%3A46;24;1366;728;0;0;;;;;;;;;;;;;;;;;;;25;";
            body["checkout.billing.billingOptions.selectedBillingOptions.giftCard.giftCardInput.giftCard"] = "";
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
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
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
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
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
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
    }

    if (curl == nullptr)
    {
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_INTERFACE, userData->m_buyResult.m_localIp.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_PRIVATE, userData);    

    if (SettingManager::getInstance()->m_enableDebug)
    {
        qInfo("begin to send request %d", userData->m_buyResult.m_currentStep);
    }

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

    if (SettingManager::getInstance()->m_enableDebug)
    {
        qInfo("receive the response of request %d", userData->m_buyResult.m_currentStep);
    }

    if (userData->m_buyResult.m_currentStep <= STEP_REVIEW)
    {
        int64_t now = GetTickCount64();
        int takeTime = int(now - userData->m_stepBeginTime);
        userData->m_buyResult.m_takeTimes.append(takeTime);
        userData->m_stepBeginTime = now;
    }

    long statusCode = 0;
    QString responseData;
    getResponse(curl, statusCode, responseData);

    // 网络请求报错，流程结束
    if (!(statusCode >= 200 && statusCode < 400))
    {
        m_buyResults.append(userData->m_buyResult);
        delete userData;
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

    bool canNextStep = true;
    if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_STORE)
    {
        userData->m_buyResult.m_currentStep = STEP_PICKUP_CONTACT;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PICKUP_CONTACT)
    {
        userData->m_buyResult.m_currentStep = STEP_BILLING;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_BILLING)
    {
        userData->m_buyResult.m_currentStep = STEP_REVIEW;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_REVIEW)
    {
        userData->m_buyResult.m_currentStep = STEP_PROCESS;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PROCESS)
    {
        if (!handleProcessResponse(userData, responseData))
        {
            userData->m_buyResult.m_currentStep = STEP_PROCESS;
        }
        else
        {
            if (userData->m_buyResult.m_success)
            {
                userData->m_buyResult.m_currentStep = STEP_QUERY_ORDER_NO;
            }
            else
            {
                canNextStep = false;
            }
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        handleQueryOrderResponse(userData, responseData);
        canNextStep = false;  // 不管有没有获取到订单号，都结束流程
    }

    // 开发调试使用
    if (userData->m_buyResult.m_currentStep >= SettingManager::getInstance()->m_stopStep)
    {
        canNextStep = false;
    }

    if (canNextStep)
    {
        CURL* nextCurl = makeBuyingRequest(userData);
        if (nextCurl)
        {
            curl_multi_add_handle(m_multiHandle, nextCurl);
        }
    }
    else
    {
        m_buyResults.append(userData->m_buyResult);
        delete userData;
    }
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
        return false;
    }

    QJsonObject root = jsonDocument.object();
    if (root.contains("body") && root["body"].toObject().contains("meta") &&
            root["body"].toObject()["meta"].toObject().contains("page") &&
            root["body"].toObject()["meta"].toObject()["page"].toObject().contains("title"))
    {
        QString title = root["body"].toObject()["meta"].toObject()["page"].toObject()["title"].toString();
        userData->m_buyResult.m_failReason = title;
        QString log = QString::fromWCharArray(L"账号(%1)下单失败")
                .arg(userData->m_buyResult.m_account);
        emit printLog(log);
        return true;
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
