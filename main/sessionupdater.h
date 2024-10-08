﻿#ifndef SESSIONUPDATER_H
#define SESSIONUPDATER_H

#include <QThread>
#include <QObject>
#include <QDateTime>
#include <QMutex>
#include <QJsonObject>
#include <QSharedPointer>
#include "httpthread.h"
#include "datamodel.h"
#include "buyparammanager.h"

// 步骤
#define STEP_CHECK_EXPIRED  1   // 检测是否过期
#define STEP_EXTEND_SESSION 2   // 延长会话

class SessionUserData
{
public:
    // 账号
    QString m_account;

    // 步骤
    int m_step = STEP_CHECK_EXPIRED;

    // 重试次数
    int m_retry = 0;

    // 更新结果
    bool m_success = false;
};


class SessionUpdater : public HttpThread
{
    Q_OBJECT

public:
    explicit SessionUpdater(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

    void setBuyParamManager(QSharedPointer<BuyParamManager> manager) { m_buyParamManager = manager; }

protected:
    void run() override;

    int getTimeOutSeconds() override { return 20; }

signals:
    void printLog(QString logContent);

    void sessionExpired();

private:
    CURL* makeSessionUpdateRequest(SessionUserData* userData);

    void handleResponse(CURL* curl);    

    void retry(CURL* curl);

private:
    bool m_requestStop = false;

    CURLM* m_multiHandle = nullptr;

    QSharedPointer<BuyParamManager> m_buyParamManager;
};

#endif // SESSIONUPDATER_H
