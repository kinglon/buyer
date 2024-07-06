#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QMap>
#include "datamodel.h"
#include "planitemwidget.h"
#include "planrunner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void initCtrls();

    void addPlanListItemCtrl(const PlanItem& plan);

    void updatePlanListItemCtrl(QString planId);

    QListWidgetItem* getPlanListItem(QString planId);

private slots:
    void onImportUserInfoBtn();

    void onAddPlanBtn();

    void onEditPlanBtn(QString planId);

    void onDeletePlanBtn(QString planId);

    void onRunPlanBtn(QString planId);

    void onStopPlanBtn(QString planId);

    void addLog(QString log);

private:
    Ui::MainWindow *ui;

    QMap<QString, PlanRunner*> m_planRunners;
};
#endif // MAINWINDOW_H
