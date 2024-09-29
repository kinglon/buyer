#ifndef PLANRUNNER_H
#define PLANRUNNER_H

#include <Windows.h>
#include <QObject>
#include <QVector>
#include "datamodel.h"
#include "goodsavailabilitycheckerbase.h"
#include "goodsbuyer.h"
#include "sessionupdater.h"

class PlanRunner : public QObject
{
    Q_OBJECT

public:
    explicit PlanRunner(QString planId, QObject *parent = nullptr);

public:
    bool start();

    void stop();

    void setLocalIps(const QVector<QString>& localIps) { m_localIps = localIps; }

    QVector<QString> getLocalIps() { return m_localIps; }

    bool isRestart() { return m_restart; }

    void mockHaveGoods();

signals:
    void log(QString content);

    void planStatusChange(QString planId);

    void runFinish(QString planId, bool success);

private:
    // 创建上货Python程序的参数文件
    bool createAddCardRunnerParamFile(PlanItem* plan, QString paramFilePath);

    // 启动python程序上货
    bool launchAddCartRunner(PlanItem* plan);

    // 查询python程序运行结果，true上号结束
    bool queryAddCartRunnerStatus();

    // 加载上号成功的数据
    bool loadAddCartResult();

    // 进入监控流程
    void enterMonitorProcess();

    // 启动是否有货检测器
    void launchGoodsChecker();

    // 启动会话更新器
    void launchSessionUpdater();

    // 启动购买器
    bool launchGoodsBuyer();

    // 保存购买结果
    bool saveBuyingResult(const QVector<BuyResult>& buyResults);

    // 删除下单成功的用户
    bool deleteSuccessUsers(const QVector<BuyResult>& buyResults);

    // 流程结束
    void finishPlan();

private slots:
    // 打印日志前加上计划名字前缀
    void printLog(const QString& content);

    // 有货
    void onGoodsCheckFinish(GoodsAvailabilityCheckerBase* checker, QVector<ShopItem>* shops);

    // 购买完成
    void onGoodsBuyFinish(GoodsBuyer* buyer, QVector<BuyResult>* buyResults);

    // 会话过期
    void onSessionExpired();

private:
    QString m_planId;

    QString m_planName;

    QString m_planDataPath;

    bool m_requestStop = false;

    QVector<GoodsAvailabilityCheckerBase*> m_goodsCheckers;

    SessionUpdater* m_sessionUpdater = nullptr;

    QVector<GoodsBuyer*> m_goodsBuyers;

    // 购买参数列表
    QVector<BuyParam> m_buyParams;

    // 购买结果列表
    QVector<BuyResult> m_buyResults;

    // 用于定时购买倒计时
    int m_elapseSeconds = 24*3600;

    // 本地IP列表
    QVector<QString> m_localIps;

    // 标志是否重启
    bool m_restart = false;
};

#endif // PLANRUNNER_H
