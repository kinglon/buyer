#ifndef PLANRUNNER_H
#define PLANRUNNER_H

#include <Windows.h>
#include <QObject>
#include <QVector>
#include "datamodel.h"
#include "goodsavailabilitychecker.h"
#include "goodsbuyer.h"

class PlanRunner : public QObject
{
    Q_OBJECT

public:
    explicit PlanRunner(QString planId, QObject *parent = nullptr);

public:
    bool start();

    void stop();

signals:
    void log(QString content);

    void planStatusChange(QString planId);

    void runFinish(QString planId, bool success);

private:
    // 获取本地IP列表
    QVector<QString> getLocalIps();

    // 创建上货Python程序的参数文件
    bool createAddCardRunnerParamFile(PlanItem* plan, QString paramFilePath);

    // 启动python程序上货
    bool launchAddCartRunner(PlanItem* plan);

    // 查询python程序运行结果，true上号结束
    bool queryAddCartRunnerStatus();

    // 加载上号成功的数据
    bool loadAddCartResult();

    // 启动是否有货检测器
    void launchGoodsChecker();

    // 启动购买器
    bool launchGoodsBuyer();

    // 保存购买结果
    bool saveBuyingResult(const QVector<BuyResult>& buyResults);

    // 流程结束
    void finishPlan();

private slots:
    // 打印日志前加上计划名字前缀
    void printLog(const QString& content);

    // 有货
    void onGoodsCheckFinish(QVector<ShopItem>* shops);

    // 购买完成
    void onGoodsBuyFinish(GoodsBuyer* buyer, QVector<BuyResult>* buyResults);

private:
    QString m_planId;

    QString m_planName;

    QString m_planDataPath;

    bool m_requestStop = false;

    GoodsAvailabilityChecker* m_goodsChecker = nullptr;

    QVector<GoodsBuyer*> m_goodsBuyers;

    // 本地IP列表
    QVector<QString> m_localIps;

    // 购买参数列表
    QVector<BuyParam> m_buyParams;

    // 购买结果列表
    QVector<BuyResult> m_buyResults;

    // 用于定时购买倒计时
    int m_elapseSeconds = 24*3600;
};

#endif // PLANRUNNER_H
