#ifndef GOODSAVAILABILITYCHECKER_H
#define GOODSAVAILABILITYCHECKER_H

#include <QThread>
#include <QObject>
#include "httpthread.h"
#include "datamodel.h"

class GoodsAvailabilityChecker : public HttpThread
{
    Q_OBJECT

public:
    explicit GoodsAvailabilityChecker(QObject *parent = nullptr);

public:
    // 请求退出
    void requestStop() { m_requestStop = true; }

    // 模拟有货
    void mockFinish() { m_mockFinish = true; }

    // 设置查询店铺列表
    void setShops(const QVector<ShopItem>& shops) { m_shops = shops; }

    // 设置查询机型码
    void setPhoneCode(QString phoneCode) { m_phoneCode = phoneCode; }

protected:
    void run() override;

    QMap<QString,QString> getCommonHeaders() override;

    int getTimeOutSeconds() override { return 10; }

private:
    // 获取本地IP列表
    QVector<QString> getLocalIps();

    void getProxyServer(QVector<ProxyServer>& proxyServers);

    void parseProxyServerData(const QString& data, QVector<ProxyServer>& proxyServers);

    QVector<ShopItem> queryIfGoodsAvailable();

    // 构造一个按邮编查询是否有货的请求，没有指定邮编就随机
    CURL* makeQueryRequest(QString postal);

    void parseQueryShopData(const QString& data, QVector<ShopItem>& shops);

signals:
    // 有货的店铺
    void checkFinish(QVector<ShopItem>* shops);

    void printLog(QString content);

private:
    bool m_requestStop = false;

    bool m_mockFinish = false;

    // 最大请求数
    int m_maxReqCount = 30;

    // 使用的请求数
    int m_reqCount = 0;

    // 请求间隔毫秒数
    int m_reqIntervalMs = 100;

    // 上一次请求发送的时间，GetTickCount64()返回的时间
    int64_t m_lastSendReqTimeMs = 0;

    // 查询店铺列表
    QVector<ShopItem> m_shops;

    // 查询机型码
    QString m_phoneCode;

    // 本地IP列表
    QVector<QString> m_localIps;

    // 可用代理IP池
    QVector<ProxyServer> m_proxyServers;

    // 轮询使用代理IP池，标识下一个使用的IP索引
    int m_nextProxyIndex = 0;

    // 轮询使用本地IP，标识下一个使用的IP索引
    int m_nextLocalIpIndex = 0;
};

#endif // GOODSAVAILABILITYCHECKER_H
