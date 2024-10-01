#ifndef GOODSAVAILABILITYCHECKERMAP_H
#define GOODSAVAILABILITYCHECKERMAP_H

#include <QThread>
#include <QObject>
#include <QSharedPointer>
#include "goodsavailabilitycheckerbase.h"
#include "buyparammanager.h"
#include "appledataparser.h"

class MapCheckerUserData
{
public:
    // 账号
    QString m_account;

    // 本地IP
    QString m_localIp;

    // 上一次发送的时间 GetTickCount64返回值
    qint64 m_lastSendTime = 0;
};

class MapCheckerReportData
{
public:
    // 请求次数
    int m_requestCount = 0;

    // 上一次报告时间
    qint64 m_lastReportTime = 0;

    // 店铺查询次数
    QMap<QString, int> m_shopQueryCount;

    // 手机是否有货
    bool m_hasPhone = false;

    // 配件是否有货
    bool m_hasRecommend = false;

public:
    void reset()
    {
        m_requestCount = 0;
        m_lastReportTime = GetTickCount64();
        for (auto it=m_shopQueryCount.begin(); it!=m_shopQueryCount.end(); it++)
        {
            it.value() = 0;
        }
        m_hasPhone = false;
        m_hasRecommend = false;
    }

    QString toString()
    {
        QString goodsDetail = AppleDataParser::getGoodsAvailabilityString(m_hasPhone, m_hasRecommend);
        QString shopQueryCountString;
        for (auto it=m_shopQueryCount.begin(); it!=m_shopQueryCount.end(); it++)
        {
            shopQueryCountString += ", " + it.key() + "=" + QString::number(it.value());
        }
        qint64 elapse = GetTickCount64() - m_lastReportTime;
        QString str = QString::fromWCharArray(L"%1, 时长=%2, 请求次数=%3, %4")
                .arg(goodsDetail, QString::number(elapse), QString::number(m_requestCount), shopQueryCountString);
        return str;
    }
};

// 采用地图页接口进行监控
class GoodsAvailabilityCheckerMap : public GoodsAvailabilityCheckerBase
{
    Q_OBJECT

public:
    explicit GoodsAvailabilityCheckerMap(QObject *parent = nullptr);

    void setBuyParamManager(QSharedPointer<BuyParamManager> manager) { m_buyParamManager = manager; }

protected:
    void run() override;

    int getTimeOutSeconds() override { return 5; }

private:
    QVector<ShopItem> queryIfGoodsAvailable();

    // 构造查询请求
    CURL* makeQueryRequest(MapCheckerUserData* userData);

    void parseQueryData(const QString& data, QVector<ShopItem>& availShops);

private:
    // 最大请求数
    int m_maxReqCount = 30;

    // 当前运行的请求数
    int m_reqCount = 0;

    // 请求间隔毫秒数
    int m_reqIntervalMs = 100;

    // 号使用间隔毫秒数
    int m_buyParamIntervalMs = 15000;

    // 上一次请求发送的时间，GetTickCount64()返回的时间
    int64_t m_lastSendReqTimeMs = 0;

    // 用于查询的店铺，会搜索附近的，所以不需要每个店铺查询
    QVector<QString> m_queryShopPostalCodes;

    // 下一个查询索引
    int m_nextQueryPostalCodeIndex = 0;

    MapCheckerReportData m_reportData;

    QSharedPointer<BuyParamManager> m_buyParamManager;
};

#endif // GOODSAVAILABILITYCHECKERMAP_H
