#ifndef FIXTIMEBUYER_H
#define FIXTIMEBUYER_H

#include <Windows.h>
#include <QString>
#include "planmanager.h"

class FixTimeBuyer
{
protected:
    FixTimeBuyer();

public:
    static FixTimeBuyer* getInstance();

public:
    bool isRunning();

    bool start(QString planId);

    void stop();

    QString getLastError() { return m_lastError; }

private:
    // 创建购买程序的参数文件
    bool createBuyerParamFile(PlanItem* plan, QString paramFilePath);

private:
    HANDLE m_hProcess = NULL;

    QString m_lastError;

    QString m_planId;
};

#endif // FIXTIMEBUYER_H
