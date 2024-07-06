#ifndef GOODSAVAILABILITYCHECKER_H
#define GOODSAVAILABILITYCHECKER_H

#include <QThread>
#include <QObject>
#include "httpthread.h"

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

    // 设置代理IP所属的国家
    void setProxyRegion(QString region) { m_proxyRegion = region; }

protected:
    void onRun() override;

    QVector<QString> getHeaders() override;

private:
    void getProxyServer(QVector<ProxyServer>& proxyServers);

    void parseProxyServerData(const QString& data, QVector<ProxyServer>& proxyServers);

signals:
    void checkFinish(bool exist);

    void printLog(QString content);

private:
    bool m_requestStop = false;

    bool m_mockFinish = false;

    QString m_proxyRegion = "jp";
};

#endif // GOODSAVAILABILITYCHECKER_H
