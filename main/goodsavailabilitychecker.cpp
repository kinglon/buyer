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

    QVector<ShopItem> availShops;
    while (!m_requestStop)
    {
        if (m_mockFinish)
        {
            availShops.append(m_shops[0]);
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
    }
    CURL* request = makeRequest(url, QMap<QString,QString>(), QMap<QString, QString>(), proxyServer);
    m_nextProxyIndex = (m_nextProxyIndex + 1) % m_proxyServers.size();
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
            if (store.toObject().contains("retailStore"))
            {
                QJsonObject retailStore = store.toObject()["retailStore"].toObject();
                if (retailStore.contains("availableNow") && retailStore["availableNow"].toBool())
                {
                    QString name = retailStore["name"].toString();
                    QString storeNumber = retailStore["storeNumber"].toString();
                    for (auto& shop : m_shops)
                    {
                        if (shop.m_name.indexOf(name) >= 0)
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
    }
}

void GoodsAvailabilityChecker::run()
{
    if (m_shops.empty() || m_phoneCode.isEmpty())
    {
        qCritical("param is wrong");
        emit checkFinish(QVector<ShopItem>());
        return;

    }

    // 获取代理IP池
    QVector<ProxyServer> proxyServers;
    getProxyServer(proxyServers);
    if (proxyServers.empty())
    {
        emit printLog(QString::fromWCharArray(L"获取代理IP池失败"));
        emit checkFinish(QVector<ShopItem>());
        return;
    }
    m_proxyServers = proxyServers;

    // 查询是否有货
    QVector<ShopItem> shops = queryIfGoodsAvailable();
    emit checkFinish(shops);
}


QVector<QString> GoodsAvailabilityChecker::getCommonHeaders()
{
    QVector<QString> headers;

    QString origin = QString("Origin: ") + APPLE_HOST;
    headers.push_back(origin);

    QString referer = QString("Referer: ") + APPLE_HOST;
    headers.push_back(referer);

    headers.push_back("Syntax: graviton");
    headers.push_back("Modelversion: v2");

    return headers;
}
