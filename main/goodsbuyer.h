#ifndef GOODSBUYER_H
#define GOODSBUYER_H

#include <QThread>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QQueue>
#include "httpthread.h"
#include "datamodel.h"
#include "appledataparser.h"

// 购买步骤
#define STEP_SELECT_SHOP        1   // 选择店铺
#define STEP_SUBMIT_SHOP        2   // 提交店铺
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
    int m_currentStep = STEP_SELECT_SHOP;

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
        if (m_currentStep == STEP_SELECT_SHOP)
        {
            return QString::fromWCharArray(L"选择店铺");
        }
        else if (m_currentStep == STEP_SUBMIT_SHOP)
        {
            return QString::fromWCharArray(L"提交店铺");
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

// 商品是否有货
enum GoodsAvailStatus
{
    UNKNOWN = 1,
    HAVE = 2,
    NOT_HAVE = 3
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

    // 标志是否无货
    int m_phoneAvailStatus = GoodsAvailStatus::UNKNOWN;

    // ssi
    QString m_ssi;

    // pltn
    QString m_pltn;

    // 领取的日期和时间
    PickupDateTime m_pickupDateTime;

    // 上一次请求的时间, GetTickCount64()返回的值
    qint64 m_lastRequestTime = 0;

    // 当前步骤重试次数
    int m_retryCount = 0;
};

class GoodsBuyer : public HttpThread
{
    Q_OBJECT

public:
    explicit GoodsBuyer(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

    void setParams(const QVector<BuyParam>& params) { m_buyParams = params; }

    void setPlanDataPath(QString planDataPath) { m_planDataPath = planDataPath; }

    void setName(QString name) { m_name = name; }

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

    void handleSelectShopResponse(BuyUserData* userData, QString& responseData);

    void handleSubmitShopResponse(BuyUserData* userData, QString& responseData);

    // 返回false表示处理中
    bool handleProcessResponse(BuyUserData* userData, QString& responseData);

    // 返回false表示未查询到
    bool handleQueryOrderResponse(BuyUserData* userData, QString& responseData);

    // 获取手机配件是否有货
    bool getGoodsAvalibility(BuyUserData* userData, QString& responseData, bool& hasPhone, bool& hasRecommend);

    // 重发请求
    void retryRequest(BuyUserData* userData);

    // 进入步骤
    void enterStep(BuyUserData* userData, int step);

    // 结束购买，有错误
    void finishBuyWithError(BuyUserData* userData, QString error);

    // 将data保存在指定的文件名下
    void saveDataToFile(const QString& data, QString fileName);

private:
    bool m_requestStop = false;

    CURLM* m_multiHandle = nullptr;

    QVector<BuyParam> m_buyParams;

    QVector<BuyResult> m_buyResults;

    // 购买计划数据目录，尾部有斜杆
    QString m_planDataPath;

    // 统计每个步骤发送的次数
    QMap<int, int> m_stepRequestCounts;

    // 待重试的请求
    QQueue<BuyUserData*> m_retryRequests;

    // 购买器名字
    QString m_name;
};

#endif // GOODSBUYER_H
