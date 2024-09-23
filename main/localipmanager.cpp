#include "localipmanager.h"

#include <QNetworkInterface>

LocalIpManager::LocalIpManager()
{
    getLocalIps();
}

LocalIpManager* LocalIpManager::getInstance()
{
    static LocalIpManager* instance = new LocalIpManager();
    return instance;
}

QVector<QString> LocalIpManager::getAllIps()
{
    QVector<QString> ips;
    for (auto it=m_localIp2IsUse.begin(); it!=m_localIp2IsUse.end(); it++)
    {
        ips.append(it.key());
    }
    return ips;
}

bool LocalIpManager::assignIps(int count, QVector<QString>& localIps)
{
    QVector<QString> ips;
    for (auto it=m_localIp2IsUse.begin(); it!=m_localIp2IsUse.end(); it++)
    {
        if (!it.value())
        {
            ips.append(it.key());
            if (ips.size() >= count)
            {
                break;
            }
        }
    }

    if (ips.size() < count)
    {
        return false;
    }

    for (auto& ip : ips)
    {
        m_localIp2IsUse[ip] = true;
    }
    localIps = ips;

    return true;
}

void LocalIpManager::releaseIps(const QVector<QString>& localIps)
{
    for (auto& ip : localIps)
    {
        m_localIp2IsUse[ip] = false;
    }
}

void LocalIpManager::getLocalIps()
{
    m_localIp2IsUse.clear();
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& networkInterface : interfaces)
    {
        // Skip loopback and inactive interfaces
        if (networkInterface.flags().testFlag(QNetworkInterface::IsLoopBack) || !networkInterface.flags().testFlag(QNetworkInterface::IsUp))
        {
            continue;
        }

        QList<QNetworkAddressEntry> addressEntries = networkInterface.addressEntries();
        for (const QNetworkAddressEntry& entry : addressEntries)
        {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
            {
                m_localIp2IsUse[entry.ip().toString()] = false;
            }
        }
    }

    qInfo("local ip count is %d", m_localIp2IsUse.size());
}
