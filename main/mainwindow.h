#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QMap>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QCloseEvent>
#include <QShortcut>
#include "datamodel.h"
#include "planitemwidget.h"
#include "planrunner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MyItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    MyItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

public:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        // Disable the item focus frame
        QStyleOptionViewItem modifiedOption = option;
        modifiedOption.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, modifiedOption, index);
    }
};

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

    bool createPlanDataDirectory(QString planName, bool tip);

    void runPlan(QString planId, bool restart);

private slots:
    void onImportUserInfoBtn();

    void onImportRecommendBtn();

    void onAddPlanBtn();

    void onEditPlanBtn(QString planId);

    void onDeletePlanBtn(QString planId);

    void onRunPlanBtn(QString planId);

    void onStopPlanBtn(QString planId);

    void addLog(QString log);

    void onCtrlDShortcut();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;

    QMap<QString, PlanRunner*> m_planRunners;

    QShortcut* m_ctrlDShortcut = nullptr;
};
#endif // MAINWINDOW_H
