#include "proxymanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include "settingmanager.h"

ProxyManager::ProxyManager(QObject *parent)
    : HttpThread{parent}
{

}

ProxyManager* ProxyManager::getInstance()
{
    static ProxyManager* instance = new ProxyManager();
    return instance;
}

ProxyServer ProxyManager::getProxyServer()
{
    ProxyServer proxyServer;
    QMutexLocker locker(&m_mutex);
    if (m_proxyServers.size() == 0)
    {
        return proxyServer;
    }

    proxyServer = m_proxyServers[m_nextProxyIndex];
    m_nextProxyIndex = (m_nextProxyIndex+1) % m_proxyServers.size();
    return proxyServer;
}

void ProxyManager::getProxyServer(QVector<ProxyServer>& proxyServers)
{
    int retryCount = 3;
    for (int i=0; i<retryCount; i++)
    {
        QString url = QString("http://api.proxy.ipidea.io/getBalanceProxyIp?big_num=900&return_type=json&lb=1&sb=0&flow=1&regions=%1&protocol=socks5")
                .arg(SettingManager::getInstance()->m_proxyRegion);
        CURL* curl = makeRequest(url, QMap<QString,QString>(), QMap<QString,QString>(), ProxyServer());
        if (curl == nullptr)
        {
            return;
        }

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, getTimeOutSeconds());

        CURLcode result = curl_easy_perform(curl);
        if(result != CURLE_OK)
        {
            qCritical("failed to perform the request of getting proxy server, error is %s", curl_easy_strerror(result));
            freeRequest(curl);
            QThread::msleep(5000);
            continue;
        }

        long statusCode = 0;
        QString data;
        getResponse(curl, statusCode, data);
        freeRequest(curl);

        if (statusCode != 200)
        {
            qCritical("failed to perform the request of getting proxy server, status code is %d, data is %s", statusCode, data.toStdString().c_str());
            QThread::msleep(5000);
            continue;
        }

        parseProxyServerData(data, proxyServers);
        return;
    }
}

void ProxyManager::parseProxyServerData(const QString& data, QVector<ProxyServer>& proxyServers)
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

void ProxyManager::run()
{
    while (true)
    {
        if (SettingManager::getInstance()->m_useProxy)
        {
            QVector<ProxyServer> proxyServers;
            getProxyServer(proxyServers);
            if (!proxyServers.empty())
            {
                QMutexLocker locker(&m_mutex);
                m_proxyServers = proxyServers;
                m_nextProxyIndex = 0;
                qInfo("successful to get proxy, count is %d", m_proxyServers.size());
            }
            else
            {
                qCritical("failed to get proxy");
            }
        }

        QThread::sleep(300);
    }
}
