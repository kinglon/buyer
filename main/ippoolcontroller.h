#ifndef IPPOOLCONTROLLER_H
#define IPPOOLCONTROLLER_H

#include <QMutex>
#include <QVector>
#include <QMap>

// 控制IP发送请求
class IpPoolController
{
protected:
    IpPoolController();

public:
    static IpPoolController* getInstance();

    void setIps(const QVector<QString>& ips);

    // 每个IP请求间隔毫秒数，必须控制不可低于该间隔发送
    void setIpReqInterval(int interval) { m_ipReqInterval = interval; }

    // 获取IP，如果获取失败，waitTime表示需要等待的时间
    QString getIp(int& waitTime);

private:
    QMutex m_mutex;

    // IP列表
    QVector<QString> m_ips;

    // 对应每个IP上次发送请求的时间 GetTickCount64
    QVector<qint64> m_lastSendTimes;

    // 下一个IP索引
    int m_nextIpIndex = 0;

    // 每个IP请求间隔毫秒数，必须控制不可低于该间隔发送
    int m_ipReqInterval = 5000;
};

#endif // IPPOOLCONTROLLER_H
