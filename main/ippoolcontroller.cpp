#include "ippoolcontroller.h"
#include <Windows.h>
#include <QThread>

IpPoolController::IpPoolController()
{

}


IpPoolController* IpPoolController::getInstance()
{
    static IpPoolController* instance = new IpPoolController();
    return instance;
}

void IpPoolController::setIps(const QVector<QString>& ips)
{
    QMutexLocker locker(&m_mutex);
    m_ips = ips;
    m_lastSendTimes.clear();
    for (int i=0; i<m_ips.size(); i++)
    {
        m_lastSendTimes.append(0);
    }
    m_nextIpIndex = 0;
}

QString IpPoolController::getIp(int& waitTime)
{
    QMutexLocker locker(&m_mutex);
    if (m_ips.size() == 0)
    {
        waitTime = 1000;
        return "";
    }

    int elapse = GetTickCount64() - m_lastSendTimes[m_nextIpIndex];
    if (elapse < m_ipReqInterval)
    {
        waitTime = m_ipReqInterval - elapse;
        return "";
    }
    else
    {
        waitTime = 0;
        int currentIpIndex = m_nextIpIndex;
        m_lastSendTimes[currentIpIndex] = GetTickCount64();
        m_nextIpIndex = (currentIpIndex+1)%m_ips.size();
        return m_ips[currentIpIndex];
    }
}
