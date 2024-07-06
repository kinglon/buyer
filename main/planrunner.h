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

    void runFinish(QString planId);

private:
    // 启动python程序上货
    bool launchAddCartRunner(PlanItem* plan);

    // 查询python程序运行结果
    void queryAddCartRunnerStatus();

    // 启动是否有货检测器
    void launchGoodsChecker();

    // 启动购买器
    void launchGoodsBuyer();

    // 流程结束
    void finishPlan();

private slots:
    // 打印日志前加上计划名字前缀
    void printLog(const QString& content);

    // 有货
    void onGoodsCheckFinish(bool exist);

    // 购买完成
    void onGoodsBuyFinish(GoodsBuyer* buyer);

private:
    QString m_planId;

    QString m_planName;

    bool m_requestStop = false;

    GoodsAvailabilityChecker* m_goodsChecker = nullptr;

    QVector<GoodsBuyer*> m_goodsBuyers;
};

#endif // PLANRUNNER_H
