#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include <QThread>
#include <QObject>
#include <QMutex>
#include "httpthread.h"

class ProxyManager : public HttpThread
{
    Q_OBJECT

protected:
    explicit ProxyManager(QObject *parent = nullptr);

public:
    static ProxyManager* getInstance();

    // 随机获取一个代理地址
    ProxyServer getProxyServer();

protected:
    void run() override;

    int getTimeOutSeconds() override { return 20; }

private:
    void getProxyServer(QVector<ProxyServer>& proxyServers);

    void parseProxyServerData(const QString& data, QVector<ProxyServer>& proxyServers);

private:
    QMutex m_mutex;

    // 可用代理IP池
    QVector<ProxyServer> m_proxyServers;

    // 轮询使用代理IP池，标识下一个使用的IP索引
    int m_nextProxyIndex = 0;
};

#endif // PROXYMANAGER_H
