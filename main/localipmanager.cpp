#include "localipmanager.h"

#include <QNetworkInterface>

LocalIpManager::LocalIpManager()
{

}

LocalIpManager* LocalIpManager::getInstance()
{
    static LocalIpManager* instance = new LocalIpManager();
    return instance;
}

void LocalIpManager::init()
{
    getLocalIps();
}

QVector<QString> LocalIpManager::getAllIps()
{
    QVector<QString> ips;
    for (auto it=m_ipCounts.begin(); it!=m_ipCounts.end(); it++)
    {
        ips.append(it->m_ip);
    }
    return ips;
}

bool LocalIpManager::assignIps(int count, QVector<QString>& localIps)
{
    // 按次数升序排序
    std::sort(m_ipCounts.begin(), m_ipCounts.end(), [](const IpCount& a, const IpCount& b) {
            return a.m_count < b.m_count;
        });

    localIps.clear();
    for (int i=0; i<count; i++)
    {
        IpCount& ipCount = m_ipCounts[i%m_ipCounts.size()];
        localIps.append(ipCount.m_ip);
        ipCount.m_count++;
    }

    return true;
}

void LocalIpManager::releaseIps(const QVector<QString>& localIps)
{
    for (auto& ip : localIps)
    {
        for (auto& ipCount : m_ipCounts)
        {
            if (ipCount.m_ip == ip)
            {
                ipCount.m_count--;
                if (ipCount.m_count < 0)
                {
                    ipCount.m_count = 0;
                }
                break;
            }
        }
    }
}

void LocalIpManager::getLocalIps()
{
    m_ipCounts.clear();
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
                IpCount ipCount;
                ipCount.m_ip = entry.ip().toString();
                m_ipCounts.append(ipCount);
            }
        }
    }

    qInfo("local ip count is %d", m_ipCounts.size());
}
