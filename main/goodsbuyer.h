#ifndef GOODSBUYER_H
#define GOODSBUYER_H

#include <QThread>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include "httpthread.h"
#include "datamodel.h"

// 购买步骤
#define STEP_QUERY_DATETIME     1   // 查询领取日期和时间
#define STEP_SELECT_SHOP        2   // 重新选择店铺
#define STEP_REVIEW             3   // 确认订单
#define STEP_PROCESS            4   // 处理订单
#define STEP_QUERY_ORDER_NO     5   // 查询订单号

class BuyResult
{
public:
    // 账号
    QString m_account;

    // 订单号
    QString m_orderNo;

    // 步骤
    int m_currentStep = STEP_QUERY_DATETIME;

    // 每个步骤的耗时，毫秒数
    QVector<QString> m_takeTimes;

    // 发送IP地址
    QString m_localIp;

    // 标志是否成功下单
    bool m_success = false;

    // 下单失败的原因
    QString m_failReason;

    // 开始购买时间，用于日期展示
    QDateTime m_beginBuyDateTime;

    // 上号的代理IP
    QString m_addCartProxy;

    // 购买店铺
    QString m_buyShopName;

public:
    QString getStepName() const
    {
        if (m_currentStep == STEP_QUERY_DATETIME)
        {
            return QString::fromWCharArray(L"查询领取日期和时间");
        }
        else if (m_currentStep == STEP_SELECT_SHOP)
        {
            return QString::fromWCharArray(L"重新选择店铺");
        }
        else if (m_currentStep == STEP_REVIEW)
        {
            return QString::fromWCharArray(L"确认订单");
        }
        else if (m_currentStep == STEP_PROCESS)
        {
            return QString::fromWCharArray(L"处理订单");
        }
        else if (m_currentStep == STEP_QUERY_ORDER_NO)
        {
            return QString::fromWCharArray(L"查询订单号");
        }
        else
        {
            return "";
        }
    }
};

class BuyUserData
{
public:
    // 输入参数
    BuyParam m_buyParam;

    // 结果
    BuyResult m_buyResult;    

    // 步骤开始时间，GetTickCount64返回的秒数
    int64_t m_stepBeginTime = 0;

    // ssi
    QString m_ssi;

    // pltn
    QString m_pltn;

    // 领取日期
    QString m_date;

    // 领取时间
    QJsonObject m_time;
};

class GoodsBuyer : public HttpThread
{
    Q_OBJECT

public:
    explicit GoodsBuyer(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

    void setParams(const QVector<BuyParam>& params) { m_buyParams = params; }    

protected:
    void run() override;

    int getTimeOutSeconds() override { return 30; }

signals:
    void printLog(QString content);

    void buyFinish(GoodsBuyer* buyer, QVector<BuyResult>* buyResults);

private:
    QString getBodyString(const QMap<QString, QString>& body);

    CURL* makeBuyingRequest(BuyUserData* userData);

    void handleResponse(CURL* curl);

    void handleQueryDateTimeResponse(BuyUserData* userData, QString& responseData);

    // 处理中，返回true表示已经处理完成(BuyUserData* userData, QString& responseData);
    bool handleProcessResponse(BuyUserData* userData, QString& responseData);

    bool handleQueryOrderResponse(BuyUserData* userData, QString& responseData);

    void getCreditCardInfo(QString cardNo, QString& cardNumberPrefix, QString& cardNoCipher);

private:
    bool m_requestStop = false;

    CURLM* m_multiHandle = nullptr;

    QVector<BuyParam> m_buyParams;

    QVector<BuyResult> m_buyResults;
};

#endif // GOODSBUYER_H
