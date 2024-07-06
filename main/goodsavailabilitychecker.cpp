#include "goodsavailabilitychecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
        QString url = QString("http://api.proxy.ipidea.io/getBalanceProxyIp?num=900&return_type=json&lb=1&sb=0&flow=1&regions=%1&protocol=socks5").arg(m_proxyRegion);
        CURL* curl = makeRequest(url, QMap<QString,QString>(), ProxyServer());
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

void GoodsAvailabilityChecker::onRun()
{
    // 获取代理IP池
    QVector<ProxyServer> proxyServers;
    getProxyServer(proxyServers);
    if (proxyServers.empty())
    {
        emit printLog(QString::fromWCharArray(L"获取代理IP池失败"));
        emit checkFinish(false);
        return;
    }


}


QVector<QString> GoodsAvailabilityChecker::getHeaders()
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
