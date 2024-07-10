#ifndef GOODSBUYER_H
#define GOODSBUYER_H

#include <QThread>
#include <QObject>
#include "httpthread.h"
#include "datamodel.h"

// 购买步骤
#define STEP_CHECKOUT_NOW       1
#define STEP_BIND_ACCOUNT       2
#define STEP_CHECKOUT_START     3
#define STEP_CHECKOUT           4
#define STEP_FULFILLMENT_RETAIL 5
#define STEP_FULFILLMENT_STORE  6
#define STEP_PICKUP_CONTACT     7
#define STEP_BILLING            8
#define STEP_REVIEW             9
#define STEP_PROCESS            10
#define STEP_QUERY_ORDER_NO     11

class BuyParam
{
public:
    // 用户资料
    UserItem m_user;

    // 购买店铺
    ShopItem m_buyingShop;

    // Cookies
    QMap<QString,QString> m_cookies;

    // x_aos_stk
    QString m_xAosStk;
};

class BuyResult
{
public:
    // 账号
    QString m_account;

    // 订单号
    QString m_orderNo;

    // 步骤
    int m_currentStep = STEP_CHECKOUT_NOW;

    // 每个步骤的耗时，毫秒数
    QVector<int> m_takeTimes;

    // 发送IP地址
    QString m_localIp;

public:
    QString getStepName() const
    {
        if (m_currentStep == STEP_CHECKOUT_NOW)
        {
            return QString::fromWCharArray(L"前往购买");
        }
        else if (m_currentStep == STEP_BIND_ACCOUNT)
        {
            return QString::fromWCharArray(L"绑定账号");
        }
        else if (m_currentStep == STEP_CHECKOUT_START)
        {
            return QString::fromWCharArray(L"开始购买1");
        }
        else if (m_currentStep == STEP_CHECKOUT)
        {
            return QString::fromWCharArray(L"开始购买2");
        }
        else if (m_currentStep == STEP_FULFILLMENT_RETAIL)
        {
            return QString::fromWCharArray(L"选择自提");
        }
        else if (m_currentStep == STEP_FULFILLMENT_STORE)
        {
            return QString::fromWCharArray(L"选择店铺");
        }
        else if (m_currentStep == STEP_PICKUP_CONTACT)
        {
            return QString::fromWCharArray(L"选择联系人");
        }
        else if (m_currentStep == STEP_BILLING)
        {
            return QString::fromWCharArray(L"支付");
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
};

class GoodsBuyer : public HttpThread
{
    Q_OBJECT

public:
    explicit GoodsBuyer(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

    void setParams(const QVector<BuyParam>& params) { m_buyParams = params; }

    void setLocalIps(const QVector<QString>& localIps) { m_localIps = localIps; }

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

    bool handleCheckNowResponse(BuyUserData* userData, QString& responseData);

    bool handleBindAccountResponse(BuyUserData* userData, QString& responseData);

    bool handleCheckoutResponse(BuyUserData* userData, QString& responseData);

    bool handleQueryOrderResponse(BuyUserData* userData, QString& responseData);

    void getCreditCardInfo(QString cardNo, QString& cardNumberPrefix, QString& cardNoCipher);

private:
    bool m_requestStop = false;

    CURLM* m_multiHandle = nullptr;

    // 本地IP列表
    QVector<QString> m_localIps;

    QVector<BuyParam> m_buyParams;

    QVector<BuyResult> m_buyResults;
};

#endif // GOODSBUYER_H
