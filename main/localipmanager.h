#ifndef LOCALIPMANAGER_H
#define LOCALIPMANAGER_H

#include <QMap>
#include <QVector>

class IpCount
{
public:
    QString m_ip;

    // 被使用的次数
    int m_count = 0;
};

class LocalIpManager
{
protected:
    LocalIpManager();

public:
    static LocalIpManager* getInstance();

    void init();

    // 获取所有IP地址
    QVector<QString> getAllIps();

    // 领取指定数量的IP，如果个数不够，返回失败
    bool assignIps(int count, QVector<QString>& localIps);

    // 释放IP，变成可用
    void releaseIps(const QVector<QString>& localIps);

private:
    // 获取本地IP列表
    void getLocalIps();

private:
    QVector<IpCount> m_ipCounts;
};

#endif // LOCALIPMANAGER_H
