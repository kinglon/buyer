#ifndef GOODSAVAILABILITYCHECKER_H
#define GOODSAVAILABILITYCHECKER_H

#include <QThread>
#include <QObject>
#include "goodsavailabilitycheckerbase.h"

// 采用首页接口进行监控
class GoodsAvailabilityChecker : public GoodsAvailabilityCheckerBase
{
    Q_OBJECT

public:
    explicit GoodsAvailabilityChecker(QObject *parent = nullptr);

public:
    // 设置查询机型码
    void setPhoneCode(QString phoneCode) { m_phoneCode = phoneCode; }

protected:
    void run() override;

    QMap<QString,QString> getCommonHeaders() override;

    int getTimeOutSeconds() override { return 5; }

private:
    QVector<ShopItem> queryIfGoodsAvailable();

    // 构造一个按邮编查询是否有货的请求，没有指定邮编就随机
    CURL* makeQueryRequest(QString postal);

    void parseQueryShopData(const QString& data, QVector<ShopItem>& shops);

private:
    // 最大请求数
    int m_maxReqCount = 30;

    // 使用的请求数
    int m_reqCount = 0;

    // 请求间隔毫秒数
    int m_reqIntervalMs = 100;

    // 上一次请求发送的时间，GetTickCount64()返回的时间
    int64_t m_lastSendReqTimeMs = 0;    

    // 查询机型码
    QString m_phoneCode;    

    // 店铺查询次数，用于统计
    QMap<QString, int> m_shopQueryCount;

    // 用于查询的店铺，会搜索附近的，所以不需要每个店铺查询
    QVector<QString> m_queryShopPostalCodes;

    // 下一个查询索引
    int m_nextQueryPostalCodeIndex = 0;
};

#endif // GOODSAVAILABILITYCHECKER_H
