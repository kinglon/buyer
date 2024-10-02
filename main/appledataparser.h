#ifndef APPLEDATAPARSER_H
#define APPLEDATAPARSER_H

#include <QString>
#include <QJsonObject>

// 商品详细信息
class GoodsDetail
{
public:
    // 店铺ID
    QString m_strStoreId;

    // 标志手机是否有货
    bool m_hasPhone = false;

    // 标志配件是否有货
    bool m_hasRecommend = false;
};

// 手机领取日期时间
class PickupDateTime
{
public:
    // 领取的日期
    QString m_date;

    // dayRadio
    QString m_dayRadio;

    // 领取的时间
    QJsonObject m_time;
};

// 苹果协议数据解析器
class AppleDataParser
{
public:
    AppleDataParser();

    // 从苹果响应数据中解析每个店铺的商品情况
    static QVector<GoodsDetail> parseGoodsDetail(const QString& data);

    static QString getGoodsAvailabilityString(bool hasPhone, bool hasRecommend);

    // 从苹果响应数据中判断商品是否都是自提
    static bool checkIfPickup(const QString& data, bool& pickup);

    // 从苹果响应数据中获取日期时间
    static bool getPickupDateTime(const QString& data, PickupDateTime& pickupDateTime);
};

#endif // APPLEDATAPARSER_H
