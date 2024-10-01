#include "appledataparser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "jsonutil.h"

AppleDataParser::AppleDataParser()
{

}

QVector<GoodsDetail> AppleDataParser::parseGoodsDetail(const QString& data)
{
    QVector<GoodsDetail> goodsDetails;
    QByteArray jsonData = data.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse goods detail");
        return goodsDetails;
    }

    QJsonObject root = jsonDocument.object();
    QVector<QString> keys;
    keys.append("body");
    keys.append("checkout");
    keys.append("fulfillment");
    keys.append("pickupTab");
    keys.append("pickup");
    keys.append("storeLocator");
    keys.append("searchResults");
    keys.append("d");
    QJsonObject dJson;
    if (!JsonUtil::findObject(root, keys, dJson) || !dJson.contains("retailStores"))
    {
        qCritical("failed to find the retailStores node");
        return goodsDetails;
    }

    QJsonArray retailStoresJson = dJson["retailStores"].toArray();
    for (auto retailStoreItem : retailStoresJson)
    {
        QJsonObject retailStoreJson = retailStoreItem.toObject();
        QString storeId = retailStoreJson["storeId"].toString();
        if (storeId.indexOf('R') == 0)
        {
            GoodsDetail goodsDetail;
            goodsDetail.m_strStoreId = storeId;
            QJsonArray lineItemAvailabilityJson = retailStoreJson["availability"].toObject()["lineItemAvailability"].toArray();
            if (lineItemAvailabilityJson.size() != 2)
            {
                qCritical("the party count is not 2");
                continue;
            }
            for (auto lineItem : lineItemAvailabilityJson)
            {
                QJsonObject lineItemJson = lineItem.toObject();
                if (lineItemJson["partName"].toString().indexOf("iPhone") >= 0)
                {
                    goodsDetail.m_hasPhone = lineItemJson["availableNowForLine"].toBool();
                }
                else
                {
                    goodsDetail.m_hasRecommend = lineItemJson["availableNowForLine"].toBool();
                }
            }
            goodsDetails.append(goodsDetail);
        }
    }

    if (goodsDetails.size() == 0)
    {
        qCritical("the count of goods detail is empty");
    }

    return goodsDetails;
}

QString AppleDataParser::getGoodsAvailabilityString(bool hasPhone, bool hasRecommend)
{
    QString str;
    if (hasPhone)
    {
        str += QString::fromWCharArray(L"手机有货");
    }
    else
    {
        str += QString::fromWCharArray(L"手机无货");
    }

    if (hasRecommend)
    {
        str += QString::fromWCharArray(L"，配件有货");
    }
    else
    {
        str += QString::fromWCharArray(L"，配件无货");
    }
    return str;
}

bool AppleDataParser::checkIfPickup(const QString& data, bool& pickup)
{
    QByteArray jsonData = data.toUtf8();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        qCritical("failed to parse data to check if pickup");
        return false;
    }

    QJsonObject root = jsonDocument.object();
    QVector<QString> keys;
    keys.append("body");
    keys.append("checkout");
    keys.append("companionBar");
    keys.append("orderDetails");
    keys.append("fulfillmentCompanionBar");
    QJsonObject fulfillmentCompanionBarJson;
    if (!JsonUtil::findObject(root, keys, fulfillmentCompanionBarJson)
            || !fulfillmentCompanionBarJson.contains("c"))
    {
        qCritical("failed to find the c node");
        return false;
    }

    QJsonArray cJson = fulfillmentCompanionBarJson["c"].toArray();
    if (cJson.size() == 0)
    {
        qCritical("c node has no items");
        return false;
    }

    pickup = true;
    for (auto cItem : cJson)
    {
        if (cItem.toString() != "pickupSummary")
        {
            pickup = false;
            break;
        }
    }

    return true;
}
