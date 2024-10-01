#ifndef APPLEDATAPARSER_H
#define APPLEDATAPARSER_H

#include <QString>

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
};

#endif // APPLEDATAPARSER_H
