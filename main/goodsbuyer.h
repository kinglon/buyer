#ifndef GOODSBUYER_H
#define GOODSBUYER_H

#include <QThread>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QQueue>
#include "httpthread.h"
#include "datamodel.h"

// 购买步骤
#define STEP_SEARCH_SHOP        1   // 搜索店铺
#define STEP_QUERY_DATETIME     2   // 查询领取日期和时间
#define STEP_SELECT_SHOP        3   // 重新选择店铺
#define STEP_REVIEW             4   // 确认订单
#define STEP_PROCESS            5   // 处理订单
#define STEP_QUERY_ORDER_NO     6   // 查询订单号

class BuyResult
{
public:
    // 账号
    QString m_account;

    // 订单号
    QString m_orderNo;

    // 步骤
    int m_currentStep = STEP_SEARCH_SHOP;

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

    void handleQueryDateTimeResponse(BuyUserData* userData, QString& responseData);

    // 获取手机配件是否有货
    bool getGoodsAvalibility(BuyUserData* userData, QString& responseData, bool& hasPhone, bool& hasRecommend);

    // 处理中，返回true表示已经处理完成，false表示需要继续查询
    bool handleProcessResponse(BuyUserData* userData, QString& responseData);

    bool handleQueryOrderResponse(BuyUserData* userData, QString& responseData);

    void getCreditCardInfo(QString cardNo, QString& cardNumberPrefix, QString& cardNoCipher);

    // 重发请求
    void retryRequest(BuyUserData* userData);

    // 进入步骤
    void enterStep(BuyUserData* userData, int step);

    QString getGoodsAvailabilityString(bool hasPhone, bool hasRecommend);

    // 将data保存在C盘指定的文件名下
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
