#ifndef BUYPARAMMANAGER_H
#define BUYPARAMMANAGER_H

#include "datamodel.h"
#include <QMutex>


class BuyParamManager
{
public:
    BuyParamManager();

public:
    void setBuyParams(const QVector<BuyParam>& buyParams);

    QVector<BuyParam> getBuyParams();

    void updateCookies(QString account, const QMap<QString, QString>& cookies);

    BuyParam getBuyParam(QString account);

    void updateShops(const QVector<ShopItem>& shops);

private:
    QMutex m_mutex;

    // 购买参数列表
    QVector<BuyParam> m_buyParams;
};

#endif // BUYPARAMMANAGER_H
