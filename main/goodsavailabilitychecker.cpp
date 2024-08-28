#include "goodsavailabilitychecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "settingmanager.h"

#define APPLE_HOST "https://www.apple.com/jp"

GoodsAvailabilityChecker::GoodsAvailabilityChecker(QObject *parent)
    : HttpThread{parent}
{

}

void GoodsAvailabilityChecker::getProxyServer(QVector<ProxyServer>& proxyServers)
{
    int retryCount = 3;
    for (int i=0; i<retryCount && !m_requestStop; i++)
    {
        QString url = QString("http://api.proxy.ipidea.io/getBalanceProxyIp?num=900&return_type=json&lb=1&sb=0&flow=1&regions=%1&protocol=socks5").arg(SettingManager::getInstance()->m_proxyRegion);
        CURL* curl = makeRequest(url, QMap<QString,QString>(), QMap<QString,QString>(), ProxyServer());
        if (curl == nullptr)
        {
            return;
        }

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        CURLcode result = curl_easy_perform(curl);
        if(result != CURLE_OK)
        {
            qCritical("failed to perform the request of getting proxy server, error is %s", curl_easy_strerror(result));
            freeRequest(curl);
            QThread::msleep(1000);
            continue;
        }

        long statusCode = 0;
        QString data;
        getResponse(curl, statusCode, data);
        freeRequest(curl);

        if (statusCode != 200)
        {
            qCritical("failed to perform the request of getting proxy server, status code is %d, data is %s", statusCode, data.toStdString().c_str());
            QThread::msleep(1000);
            continue;
        }

        parseProxyServerData(data, proxyServers);
        return;
    }
}

void GoodsAvailabilityChecker::parseProxyServerData(const QString& data, QVector<ProxyServer>& proxyServers)
{
    QByteArray jsonData = data.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse json, data is %s", data.toStdString().c_str());
        return;
    }

    QJsonObject root = jsonDocument.object();
    if (!root.contains("success") || !root["success"].toBool() || !root.contains("data"))
    {
        qCritical("the proxy server report error, data is %s", data.toStdString().c_str());
        if (root.contains("msg"))
        {
            QString msg = root["msg"].toString();
            emit printLog(msg.toStdString().c_str());
        }
        return;
    }

    QJsonArray proxyArray = root["data"].toArray();
    for (auto proxy : proxyArray)
    {
        QJsonObject proxyJson = proxy.toObject();
        ProxyServer proxyItem;
        proxyItem.m_ip = proxyJson["ip"].toString();
        proxyItem.m_port = proxyJson["port"].toInt();
        proxyServers.append(proxyItem);
    }
}

QVector<ShopItem> GoodsAvailabilityChecker::queryIfGoodsAvailable()
{
    CURLM* multiHandle = curl_multi_init();
    if (multiHandle == nullptr)
    {
        qCritical("failed to init a multi handle");
        return QVector<ShopItem>();
    }

    // 每个店铺都发送一个请求
    for (const auto& shop : m_shops)
    {
        CURL* curl = makeQueryRequest(shop.m_postalCode);
        if (curl == nullptr)
        {
            break;
        }
        curl_multi_add_handle(multiHandle, curl);
    }

    int64_t lastReportLogTime = GetTickCount64();

    QVector<ShopItem> availShops;
    while (!m_requestStop)
    {
        if (m_mockFinish)
        {
            availShops.append(m_shops[0]);
            break;
        }

        int numfds = 0;
        curl_multi_wait(multiHandle, NULL, 0, 100, &numfds);

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
                    parseQueryShopData(data, availShops);
                }
            }

            curl_multi_remove_handle(multiHandle, m->easy_handle);
            freeRequest(m->easy_handle);
            m_reqCount = max(m_reqCount-1, 0);

            if (!availShops.empty())
            {
                break;
            }
            else
            {
                CURL* curl = makeQueryRequest("");
                if (curl)
                {
                    curl_multi_add_handle(multiHandle, curl);
                }
            }
        }

        if (!availShops.empty())
        {
            break;
        }

        // 超过间隔时间没有再发送，继续发送
        if (GetTickCount64() - m_lastSendReqTimeMs > m_reqIntervalMs)
        {
            CURL* curl = makeQueryRequest("");
            if (curl)
            {
                curl_multi_add_handle(multiHandle, curl);
            }
        }

        if (GetTickCount64() - lastReportLogTime >= 5000)
        {
            emit printLog(QString::fromWCharArray(L"店铺没货"));
            lastReportLogTime = GetTickCount64();
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

CURL* GoodsAvailabilityChecker::makeQueryRequest(QString postal)
{
    if (m_reqCount >= m_maxReqCount)
    {
        return nullptr;
    }

    if (postal.isEmpty())
    {
        int randIndex = rand() % m_shops.size();
        postal = m_shops[randIndex].m_postalCode;
    }

    QString url = APPLE_HOST + QString("/shop/fulfillment-messages?pl=true&mts.0=regular&cppart=UNLOCKED_JP&parts.0=%1/A&location=%2").arg(m_phoneCode, postal);
    ProxyServer proxyServer;
    if (SettingManager::getInstance()->m_useProxy)
    {
        proxyServer = m_proxyServers[m_nextProxyIndex];
        m_nextProxyIndex = (m_nextProxyIndex + 1) % m_proxyServers.size();
    }

    CURL* request = makeRequest(url, QMap<QString,QString>(), QMap<QString, QString>(), proxyServer);    
    m_reqCount += 1;
    m_lastSendReqTimeMs = GetTickCount64();
    return request;
}

void GoodsAvailabilityChecker::parseQueryShopData(const QString& data, QVector<ShopItem>& shops)
{
    QByteArray jsonData = data.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse json, data is %s", data.toStdString().c_str());
        return;
    }

    QJsonObject root = jsonDocument.object();
    if (root.contains("body") && root["body"].toObject().contains("content") &&
            root["body"].toObject()["content"].toObject().contains("pickupMessage") &&
            root["body"].toObject()["content"].toObject()["pickupMessage"].toObject().contains("stores"))
    {
        QJsonArray stores = root["body"].toObject()["content"].toObject()["pickupMessage"].toObject()["stores"].toArray();
        for (auto store : stores)
        {
            QJsonObject storeJsonObj = store.toObject();
            if (!storeJsonObj.contains("storeNumber"))
            {
                continue;
            }
            QString storeNumber = storeJsonObj["storeNumber"].toString();

            if (!storeJsonObj.contains("address") || !storeJsonObj["address"].toObject().contains("postalCode"))
            {
                continue;
            }
            QString postalCode = storeJsonObj["address"].toObject()["postalCode"].toString();

            QString parts = m_phoneCode + "/A";
            if (!storeJsonObj.contains("partsAvailability")
                    || !storeJsonObj["partsAvailability"].toObject().contains(parts))
            {
                continue;
            }

            QJsonObject partsJsonObject = storeJsonObj["partsAvailability"].toObject()[parts].toObject();
            if (!partsJsonObject.contains("pickupDisplay") || partsJsonObject["pickupDisplay"].toString() != "available")
            {
                continue;
            }

            for (auto& shop : m_shops)
            {
                if (shop.m_postalCode == postalCode)
                {
                    ShopItem shopItem = shop;
                    shopItem.m_storeNumber = storeNumber;
                    shops.append(shopItem);
                    break;
                }
            }
        }
    }
}

void GoodsAvailabilityChecker::run()
{
    if (m_shops.empty() || m_phoneCode.isEmpty())
    {
        qCritical("param is wrong");
        emit checkFinish(nullptr);
        return;

    }

    m_reqIntervalMs = SettingManager::getInstance()->m_queryGoodInterval;
    m_maxReqCount = getTimeOutSeconds() * 1000 / m_reqIntervalMs;

    // 获取代理IP池
    if (SettingManager::getInstance()->m_useProxy)
    {
        QVector<ProxyServer> proxyServers;
        getProxyServer(proxyServers);
        if (proxyServers.empty())
        {
            emit printLog(QString::fromWCharArray(L"获取代理IP池失败"));
            emit checkFinish(nullptr);
            return;
        }
        m_proxyServers = proxyServers;
    }

    // 查询是否有货
    QVector<ShopItem> shops = queryIfGoodsAvailable();
    emit checkFinish(new QVector<ShopItem>(shops));
}


QMap<QString,QString> GoodsAvailabilityChecker::getCommonHeaders()
{
    QMap<QString,QString> headers;
    headers["Origin"] = APPLE_HOST;
    headers["Referer"] = APPLE_HOST;
    headers["Syntax"] = "graviton";
    headers["Modelversion"] = "v2";

    return headers;
}
