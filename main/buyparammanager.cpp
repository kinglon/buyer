#include "buyparammanager.h"

BuyParamManager::BuyParamManager()
{

}

void BuyParamManager::setBuyParams(const QVector<BuyParam>& buyParams)
{
    QMutexLocker locker(&m_mutex);
    m_buyParams = buyParams;
}

QVector<BuyParam> BuyParamManager::getBuyParams()
{
    QMutexLocker locker(&m_mutex);
    return m_buyParams;
}

void BuyParamManager::updateCookies(QString account, const QMap<QString, QString>& cookies)
{
    QMutexLocker locker(&m_mutex);
    for (auto& buyParam : m_buyParams)
    {
        if (buyParam.m_user.m_accountName == account)
        {
            for (auto it=cookies.begin(); it!=cookies.end(); it++)
            {
                if (it.value() == "DELETED")
                {
                    buyParam.m_cookies.remove(it.key());
                }
                else
                {
                    buyParam.m_cookies[it.key()] = it.value();
                }
            }
            break;
        }
    }
}

BuyParam BuyParamManager::getBuyParam(QString account)
{
    QMutexLocker locker(&m_mutex);
    for (auto& buyParam : m_buyParams)
    {
        if (buyParam.m_user.m_accountName == account)
        {
            return buyParam;
        }
    }

    qCritical("failed to get the cookies of %s", account.toStdString().c_str());
    return BuyParam();
}

void BuyParamManager::updateShops(const QVector<ShopItem>& shops)
{
    QMutexLocker locker(&m_mutex);
    for (int i=0; i<m_buyParams.size(); i++)
    {
        m_buyParams[i].m_buyingShop = shops[i%shops.size()];
    }
}
