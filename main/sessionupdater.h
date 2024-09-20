#ifndef SESSIONUPDATER_H
#define SESSIONUPDATER_H

#include <QThread>
#include <QObject>
#include <QDateTime>
#include <QMutex>
#include <QJsonObject>
#include "httpthread.h"
#include "datamodel.h"

// 步骤
#define STEP_SESSION1   1
#define STEP_SESSION2   2

class SessionUserData
{
public:
    // 输入参数
    BuyParam m_buyParam;

    // 步骤
    int m_step = STEP_SESSION1;
};


class SessionUpdater : public HttpThread
{
    Q_OBJECT

public:
    explicit SessionUpdater(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

    void setParams(const QVector<BuyParam>& params) { m_buyParams = params; }

    QVector<BuyParam>& getBuyParams();

protected:
    void run() override;

    int getTimeOutSeconds() override { return 20; }

private:
    CURL* makeSessionUpdateRequest(SessionUserData* userData);

    void handleResponse(CURL* curl);    

private:
    QMutex m_mutex;

    bool m_requestStop = false;

    CURLM* m_multiHandle = nullptr;

    QVector<BuyParam> m_buyParams;
};

#endif // SESSIONUPDATER_H
