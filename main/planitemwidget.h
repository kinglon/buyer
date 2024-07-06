#ifndef PLANITEMWIDGET_H
#define PLANITEMWIDGET_H

#include <QWidget>
#include "datamodel.h"

namespace Ui {
class PlanItemWidget;
}

class PlanItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanItemWidget(const QString& planId, QWidget *parent = nullptr);
    ~PlanItemWidget();

public:
    void updateCtrls();

private:
    void initCtrls();

    bool isRunning();

signals:
    void deletePlan(QString planId);

    void editPlan(QString planId);

    void runPlan(QString planId);

    void stopPlan(QString planId);

private:
    Ui::PlanItemWidget *ui;

    QString m_planId;
};

#endif // PLANITEMWIDGET_H
