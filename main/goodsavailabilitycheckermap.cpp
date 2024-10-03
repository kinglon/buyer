#include "goodsavailabilitycheckermap.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include "settingmanager.h"
#include "ippoolcontroller.h"
#include "appledataparser.h"

#define APPLE_HOST "https://www.apple.com/jp"

GoodsAvailabilityCheckerMap::GoodsAvailabilityCheckerMap(QObject *parent)
    : GoodsAvailabilityCheckerBase{parent}
{
    m_queryShopPostalCodes.push_back("160-0022");
    m_queryShopPostalCodes.push_back("100-0005");
    m_queryShopPostalCodes.push_back("600-8006");
    m_queryShopPostalCodes.push_back("212-0013");
    m_queryShopPostalCodes.push_back("542-0086");
    m_queryShopPostalCodes.push_back("460-0008");
    m_queryShopPostalCodes.push_back("810-0001");

    m_maxReqCount = getTimeOutSeconds() * 1000 / SettingManager::getInstance()->m_queryGoodInterval;
}

QVector<ShopItem> GoodsAvailabilityCheckerMap::queryIfGoodsAvailable()
{
    CURLM* multiHandle = curl_multi_init();
    if (multiHandle == nullptr)
    {
        qCritical("failed to init a multi handle");
        return QVector<ShopItem>();
    }    

    // 初始化每个店铺查询次数
    for (const auto& shop : m_shops)
    {
        m_reportData.m_shopQueryCount[shop.m_storeNumber] = 0;
    }

    // 初始化每个号的user data
    QVector<MapCheckerUserData> userDatas;
    QVector<BuyParam> buyParams = m_buyParamManager->getBuyParams();
    for (int i=0; i<buyParams.size(); i++)
    {
        MapCheckerUserData userData;
        userData.m_account = buyParams[i].m_user.m_accountName;
        userDatas.append(userData);
    }

    // 该间隔可以确保每个号按要求的时间内才会轮询到一次
    int eachBuyInterval = m_buyParamIntervalMs / userDatas.size();
    int reqIntervalMs = max(eachBuyInterval, SettingManager::getInstance()->m_queryGoodInterval);

    // 下一个用于发送的号索引
    int nextBuyParamIndex = 0;

    m_reportData.m_lastReportTime = GetTickCount64();

    QVector<ShopItem> availShops;
    while (!m_requestStop)
    {
        if (m_mockFinish)
        {
            availShops.append(m_shops[0]);
            break;
        }

        // 发送请求
        qint64 now = GetTickCount64();
        qint64 buyParamElapse = now - userDatas[nextBuyParamIndex].m_lastSendTime;
        qint64 requestElapse = now - m_lastSendReqTimeMs;
        if (buyParamElapse >= m_buyParamIntervalMs && requestElapse >= reqIntervalMs)
        {
            int waitTime = 0;
            QString localIp = IpPoolController::getInstance()->getIp(waitTime);
            if (!localIp.isEmpty())
            {
                userDatas[nextBuyParamIndex].m_localIp = localIp;
                CURL* curl = makeQueryRequest(&userDatas[nextBuyParamIndex]);
                if (curl)
                {
                    curl_multi_add_handle(multiHandle, curl);
                    m_reqCount++;
                    m_reportData.m_requestCount++;
                    userDatas[nextBuyParamIndex].updateSendTime();
                    m_lastSendReqTimeMs = GetTickCount64();
                    nextBuyParamIndex = (nextBuyParamIndex+1)%userDatas.size();
                }
            }
        }

        int numfds = 0;
        curl_multi_wait(multiHandle, NULL, 0, 10, &numfds);

        int stillRunning = 0;
        CURLMcode mc = curl_multi_perform(multiHandle, &stillRunning);
        if (mc)
        {
            qCritical("curl_multi_perform return error: %d", mc);
            break;
        }

        while (true)
        {
            int msgs_left = 0;
            CURLMsg *m = curl_multi_info_read(multiHandle, &msgs_left);
            if (m == nullptr)
            {
                break;
            }

            if (m->msg != CURLMSG_DONE)
            {
                continue;
            }

            if (m->data.result == CURLE_OK)
            {
                long statusCode = 0;
                QString data;
                getResponse(m->easy_handle, statusCode, data);
                if (statusCode == 200)
                {                    
                    handleResponse(m->easy_handle, data, availShops);
                }
                else
                {
                    qCritical("failed to do query goods request, status is %d", statusCode);
                }
            }
            else
            {
                qCritical("failed to send query goods request, error is %d", m->data.result);
            }

            curl_multi_remove_handle(multiHandle, m->easy_handle);
            freeRequest(m->easy_handle);
            m_reqCount = max(m_reqCount-1, 0);

            if (!availShops.empty())
            {
                break;
            }           
        }

        if (!availShops.empty())
        {
            break;
        }

        // 记录报告
        int elapse = GetTickCount64() - m_reportData.m_lastReportTime;
        if (elapse >= 15000)
        {
            QString logContent = m_reportData.toString() + QString::fromWCharArray(L", 处理中请求次数=%1").arg(m_reqCount);
            emit printLog(logContent);
            m_reportData.reset();
        }
    }

    // 释放所有请求
    CURL **list = curl_multi_get_handles(multiHandle);
    if (list)
    {
        for (int i = 0; list[i]; i++)
        {
            curl_multi_remove_handle(multiHandle, list[i]);
            freeRequest(list[i]);
        }
        curl_free(list);
    }
    curl_multi_cleanup(multiHandle);

    return availShops;
}

CURL* GoodsAvailabilityCheckerMap::makeQueryRequest(MapCheckerUserData* userData)
{
    if (m_reqCount >= m_maxReqCount)
    {
        return nullptr;
    }

    BuyParam buyParam = m_buyParamManager->getBuyParam(userData->m_account);

    QMap<QString, QString> headers;
    headers["origin"] = APPLE_HOST;
    headers["Referer"] = APPLE_HOST;
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";

    QString url = buyParam.m_appStoreHost
            + QString("/shop/checkoutx/fulfillment?_a=search&_m=checkout.fulfillment.pickupTab.pickup.storeLocator");
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    headers["X-Aos-Stk"] = buyParam.m_xAosStk;
    headers["X-Requested-With"] = "Fetch";
    headers["X-Aos-Model-Page"] = "checkoutPage";
    headers["Referer"] = buyParam.m_appStoreHost
            + "/shop/checkout?_s=Fulfillment-init";
    CURL* curl = makeRequest(url, headers, buyParam.m_cookies, ProxyServer());
    if (curl)
    {
        QString storePostalCode = m_queryShopPostalCodes[m_nextQueryPostalCodeIndex];
        m_nextQueryPostalCodeIndex = (m_nextQueryPostalCodeIndex+1)%m_queryShopPostalCodes.size();
        QString body = QString("checkout.fulfillment.pickupTab.pickup.storeLocator.showAllStores=false&checkout.fulfillment.pickupTab.pickup.storeLocator.selectStore=&checkout.fulfillment.pickupTab.pickup.storeLocator.searchInput=%1")
                .arg(storePostalCode);
        setPostMethod(curl, body);
        userData->m_shopPostalCode = storePostalCode;
        curl_easy_setopt(curl, CURLOPT_INTERFACE, userData->m_localIp.toStdString().c_str());
        curl_easy_setopt(curl, CURLOPT_PRIVATE, userData);
    }

    return curl;
}

void GoodsAvailabilityCheckerMap::handleResponse(CURL* curl, const QString& data, QVector<ShopItem>& shops)
{
    MapCheckerUserData* userData = nullptr;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &userData);
    if (userData == nullptr)
    {
        return;
    }

    // 更新cookies
    QMap<QString, QString> cookies = getCookies(curl);
    m_buyParamManager->updateCookies(userData->m_account, cookies);

    QVector<GoodsDetail> goodsDetails = AppleDataParser::parseGoodsDetail(data);
    if (goodsDetails.size() == 0)
    {
        qCritical("failed to parse goods detail for %s using %s",
                  userData->m_shopPostalCode.toStdString().c_str(),
                  userData->m_account.toStdString().c_str());
        if (m_errorFileIndex < 5)
        {
            m_errorFileIndex++;
            QString errorFileName = QString::fromWCharArray(L"goods_detail_empty_%1").arg(m_errorFileIndex);
            saveDataToFile(data, errorFileName);
        }
        return;
    }

    for (auto& goodsDetail : goodsDetails)
    {
        for (auto& shop : m_shops)
        {
            if (shop.m_storeNumber == goodsDetail.m_strStoreId)
            {
                m_reportData.m_shopQueryCount[shop.m_storeNumber] += 1;
                if (goodsDetail.m_hasPhone && goodsDetail.m_hasRecommend)
                {
                    QString time;
                    while (!userData->m_sendTimes.empty())
                    {
                        time += ", " + userData->m_sendTimes.front();
                        userData->m_sendTimes.pop_front();
                    }
                    qInfo("%s, %s, %s has goods%s",
                          shop.m_name.toStdString().c_str(),
                          shop.m_storeNumber.toStdString().c_str(),
                          shop.m_postalCode.toStdString().c_str(),
                          time.toStdString().c_str());
                    shops.append(shop);
                }
                break;
            }
        }

        if (goodsDetail.m_hasPhone)
        {
            m_reportData.m_hasPhone = true;
        }

        if (goodsDetail.m_hasRecommend)
        {
            m_reportData.m_hasRecommend = true;
        }
    }
}

void GoodsAvailabilityCheckerMap::saveDataToFile(const QString& data, QString fileName)
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

void GoodsAvailabilityCheckerMap::run()
{
    // 查询是否有货
    QVector<ShopItem> shops = queryIfGoodsAvailable();
    emit checkFinish(this, new QVector<ShopItem>(shops));
}
