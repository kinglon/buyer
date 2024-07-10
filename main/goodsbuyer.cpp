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

#define APPLE_HOST "https://www.apple.com/jp"
#define APPSTORE_HOST "https://secure6.store.apple.com/jp"

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
    for (int i=0; i<m_buyParams.size(); i++)
    {
        BuyUserData* userData = new BuyUserData();
        userData->m_buyResult.m_account = m_buyParams[i].m_user.m_accountName;
        userData->m_buyResult.m_currentStep = STEP_CHECKOUT_NOW;
        userData->m_buyResult.m_localIp = m_localIps[i%m_localIps.size()];
        userData->m_buyParam = m_buyParams[i];        
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
        if (bodyStr.isEmpty())
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
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";
    headers["X-Requested-With"] = "Fetch";
    headers["X-Aos-Stk"] = userData->m_buyParam.m_xAosStk;

    CURL* curl = nullptr;
    if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT_NOW)
    {
        QString url = QString(APPLE_HOST) + "/shop/bagx/checkout_now?_a=checkout&_m=shoppingCart.actions";
        headers["X-Aos-Model-Page"] = "cart";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "shoppingCart.recommendations.recommendedItem.part=&shoppingCart.bagSavedItems.part=&shoppingCart.bagSavedItems.listId=&shoppingCart.bagSavedItems.childPart=&shoppingCart.items.item-47e7a306-7cfd-4722-b440-1b6ba32b1647.isIntentToGift=false&shoppingCart.items.item-47e7a306-7cfd-4722-b440-1b6ba32b1647.itemQuantity.quantity=1&shoppingCart.locationConsent.locationConsent=false&shoppingCart.summary.promoCode.promoCode=&shoppingCart.actions.fcscounter=&shoppingCart.actions.fcsdata=");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_BIND_ACCOUNT)
    {
        QString url = APPSTORE_HOST + QString("/shop/signIn/idms/authx?ssi=%1&up=true").arg(userData->m_ssi);
        headers["X-Aos-Model-Page"] = "signInPage";
        userData->m_buyParam.m_cookies["as_dc"] = "ucp3";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "deviceID=TF1%3B015%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3BMozilla%3BNetscape%3B5.0%2520%2528Windows%2520NT%252010.0%253B%2520Win64%253B%2520x64%2529%2520AppleWebKit%2F537.36%2520%2528KHTML%252C%2520like%2520Gecko%2529%2520Chrome%2F122.0.0.0%2520Safari%2F537.36%3B20030107%3Bundefined%3Btrue%3B%3Btrue%3BWin32%3Bundefined%3BMozilla%2F5.0%2520%2528Windows%2520NT%252010.0%253B%2520Win64%253B%2520x64%2529%2520AppleWebKit%2F537.36%2520%2528KHTML%252C%2520like%2520Gecko%2529%2520Chrome%2F122.0.0.0%2520Safari%2F537.36%3Bzh-CN%3Bundefined%3Bsecure6.store.apple.com%3Bundefined%3Bundefined%3Bundefined%3Bundefined%3Bfalse%3Bfalse%3B1719370448096%3B8%3B2005%2F6%2F7%252021%253A33%253A44%3B1366%3B768%3B%3B%3B%3B%3B%3B%3B%3B-480%3B-480%3B2024%2F6%2F26%252010%253A54%253A08%3B24%3B1366%3B728%3B0%3B0%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B25%3B&grantCode=");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT_START)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkout/start");
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            QString body = QString("pltn=%1").arg(userData->m_pltn);
            setPostMethod(curl, body);
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkout");
        headers.remove("X-Aos-Stk");
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
    }
    else if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_RETAIL)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkoutx/fulfillment?_a=selectFulfillmentLocationAction&_m=checkout.fulfillment.fulfillmentOptions");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Fulfillment-init";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "checkout.fulfillment.fulfillmentOptions.selectFulfillmentLocation=RETAIL");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_STORE)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkoutx/fulfillment?_a=continueFromFulfillmentToPickupContact&_m=checkout.fulfillment");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Fulfillment-init";
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
        QString url = APPSTORE_HOST + QString("/shop/checkoutx?_a=continueFromPickupContactToBilling&_m=checkout.pickupContact");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=PickupContact-init";
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
        QString url = APPSTORE_HOST + QString("/shop/checkoutx/billing?_a=continueFromBillingToReview&_m=checkout.billing");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Billing-init";
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
        QString url = APPSTORE_HOST + QString("/shop/checkoutx/review?_a=continueFromReviewToProcess&_m=checkout.review.placeOrder");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Review";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PROCESS)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkoutx?_a=pollingProcess&_m=spinner");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Process";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_PROCESS)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkoutx?_a=pollingProcess&_m=spinner");
        headers["X-Aos-Model-Page"] = "checkoutPage";
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Process";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
        if (curl)
        {
            setPostMethod(curl, "");
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        QString url = APPSTORE_HOST + QString("/shop/checkout/thankyou");
        headers["Referer"] = "https://secure6.store.apple.com/jp/shop/checkout?_s=Process";
        curl = makeRequest(url, headers, userData->m_buyParam.m_cookies, ProxyServer());
    }

    if (curl == nullptr)
    {
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_INTERFACE, userData->m_buyResult.m_localIp.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_PRIVATE, userData);
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

    int64_t now = GetTickCount64();
    int takeTime = int(now - userData->m_stepBeginTime);
    userData->m_buyResult.m_takeTimes.append(takeTime);
    userData->m_stepBeginTime = now;

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
        userData->m_buyParam.m_cookies[it.key()] = it.value();
    }

    bool canNextStep = true;
    if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT_NOW)
    {
        canNextStep = handleCheckNowResponse(userData, responseData);
        if (canNextStep)
        {
            userData->m_buyResult.m_currentStep = STEP_BIND_ACCOUNT;
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_BIND_ACCOUNT)
    {
        userData->m_buyParam.m_cookies.remove("myacinfo");
        canNextStep = handleBindAccountResponse(userData, responseData);
        if (canNextStep)
        {
            userData->m_buyResult.m_currentStep = STEP_CHECKOUT_START;
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT_START)
    {
        userData->m_buyResult.m_currentStep = STEP_CHECKOUT;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_CHECKOUT)
    {
        canNextStep = handleCheckoutResponse(userData, responseData);
        if (canNextStep)
        {
            userData->m_buyResult.m_currentStep = STEP_FULFILLMENT_RETAIL;
        }
    }
    else if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_RETAIL)
    {
        userData->m_buyResult.m_currentStep = STEP_FULFILLMENT_STORE;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_FULFILLMENT_STORE)
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
        userData->m_buyResult.m_currentStep = STEP_QUERY_ORDER_NO;
    }
    else if (userData->m_buyResult.m_currentStep == STEP_QUERY_ORDER_NO)
    {
        handleQueryOrderResponse(userData, responseData);
        canNextStep = false;  // 不管有没有获取到订单号，都结束流程
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

bool GoodsBuyer::handleCheckNowResponse(BuyUserData* userData, QString& responseData)
{
    QByteArray jsonData = responseData.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse json, data is %s", responseData.toStdString().c_str());
        return false;
    }

    QJsonObject root = jsonDocument.object();
    if (root.contains("head") && root["head"].toObject().contains("data") &&
            root["head"].toObject()["data"].toObject().contains("url"))
    {
        QString urlstring = root["head"].toObject()["data"].toObject()["url"].toString();
        QUrl url(urlstring);
        QUrlQuery urlQuery(url.query());
        QString ssi = urlQuery.queryItemValue("ssi");
        if (!ssi.isEmpty())
        {
            userData->m_ssi = ssi;
            return true;
        }
    }

    return false;
}

bool GoodsBuyer::handleBindAccountResponse(BuyUserData* userData, QString& responseData)
{
    QByteArray jsonData = responseData.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse json, data is %s", responseData.toStdString().c_str());
        return false;
    }

    QJsonObject root = jsonDocument.object();
    if (root.contains("head") && root["head"].toObject().contains("data") &&
            root["head"].toObject()["data"].toObject().contains("args") &&
            root["head"].toObject()["data"].toObject()["args"].toObject().contains("pltn"))
    {
        QString pltn = root["head"].toObject()["data"].toObject()["args"].toObject()["pltn"].toString();
        if (!pltn.isEmpty())
        {
            userData->m_pltn = pltn;
            return true;
        }
    }

    return false;
}

bool GoodsBuyer::handleCheckoutResponse(BuyUserData* userData, QString& responseData)
{
    int begin = responseData.indexOf("x-aos-stk\":");
    if (begin >= 0)
    {
        begin = responseData.indexOf('"', begin + strlen("x-aos-stk\":"));
        if (begin > 0)
        {
            int end = responseData.indexOf('"', begin + 1);
            QString xAosStk = responseData.mid(begin + 1, end - begin -1);
            if (!xAosStk.isEmpty())
            {
                userData->m_buyParam.m_xAosStk = xAosStk;
                return true;
            }
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
    CryptoPP::StringSource publicKeySource(publicKeyBytes.data(), publicKeyBytes.length());
    CryptoPP::RSA::PublicKey rsaPublicKey;
    rsaPublicKey.BERDecode(publicKeySource);

    CryptoPP::AutoSeededRandomPool rng;
    CryptoPP::RSAES< CryptoPP::OAEP<CryptoPP::SHA256> >::Encryptor encryptor(rsaPublicKey);
    std::string ciphertext;
    CryptoPP::StringSource(cardNo.toUtf8(), true,
        new CryptoPP::PK_EncryptorFilter(rng, encryptor,
            new CryptoPP::StringSink(ciphertext)
        )
    );

    cardNoCipher = QString("{\"cipherText\":\"%1\",\"publicKeyHash\":\"DsCuZg+6iOaJUKt5gJMdb6rYEz9BgEsdtEXjVc77oAs=\"}")
            .arg(ciphertext.c_str());
}
