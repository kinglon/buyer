#ifndef GOODSAVAILABILITYCHECKERBASE_H
#define GOODSAVAILABILITYCHECKERBASE_H

#include "httpthread.h"
#include "datamodel.h"

class GoodsAvailabilityCheckerBase : public HttpThread
{
    Q_OBJECT
public:
    explicit GoodsAvailabilityCheckerBase(QObject *parent = nullptr);

public:
    // 请求退出
    void requestStop() { m_requestStop = true; }

    // 模拟有货
    void mockFinish() { m_mockFinish = true; }

    // 设置查询店铺列表
    void setShops(const QVector<ShopItem>& shops) { m_shops = shops; }

    // 设置本地IP列表
    void setLocalIps(const QVector<QString>& localIps) { m_localIps = localIps; }

    void setPlanDataPath(QString planDataPath) { m_planDataPath = planDataPath; }

signals:
    // 有货的店铺
    void checkFinish(GoodsAvailabilityCheckerBase* checker, QVector<ShopItem>* shops);

    void printLog(QString content);

protected:
    bool m_requestStop = false;

    bool m_mockFinish = false;

    // 查询店铺列表
    QVector<ShopItem> m_shops;

    // 本地IP列表
    QVector<QString> m_localIps;

    // 轮询使用本地IP，标识下一个使用的IP索引
    int m_nextLocalIpIndex = 0;

    // 购买计划数据目录，尾部没有斜杆
    QString m_planDataPath;
};

#endif // GOODSAVAILABILITYCHECKERBASE_H
