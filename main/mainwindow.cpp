#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "plandialog.h"
#include "planmanager.h"
#include "planitemwidget.h"
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

    initCtrls();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initCtrls()
{
    for (const auto& plan : PlanManager::getInstance()->m_plans)
    {
        addPlanListItemCtrl(plan);
    }

    connect(ui->addPlanBtn, &QPushButton::clicked, [this]() {
        onAddPlanBtn();
    });

    connect(ui->selectUserInfoBtn, &QPushButton::clicked, [this]() {
        onImportUserInfoBtn();
    });
}

void MainWindow::addPlanListItemCtrl(const PlanItem& plan)
{
    QListWidgetItem *item = new QListWidgetItem(ui->planList);
    item->setData(Qt::UserRole, plan.m_id);

    PlanItemWidget* itemWidget = new PlanItemWidget(plan.m_id, ui->planList);
    connect(itemWidget, &PlanItemWidget::editPlan, this, &MainWindow::onEditPlanBtn, Qt::QueuedConnection);
    connect(itemWidget, &PlanItemWidget::deletePlan, this, &MainWindow::onDeletePlanBtn, Qt::QueuedConnection);
    connect(itemWidget, &PlanItemWidget::runPlan, this, &MainWindow::onRunPlanBtn, Qt::QueuedConnection);
    connect(itemWidget, &PlanItemWidget::stopPlan, this, &MainWindow::onStopPlanBtn, Qt::QueuedConnection);

    ui->planList->addItem(item);
    ui->planList->setItemWidget(item, itemWidget);
}

void MainWindow::updatePlanListItemCtrl(QString planId)
{
    QListWidgetItem* item = getPlanListItem(planId);
    if (item)
    {
        PlanItemWidget* itemWidget = (PlanItemWidget*)(ui->planList->itemWidget(item));
        if (itemWidget)
        {
            itemWidget->updateCtrls();
        }
    }
}

QListWidgetItem* MainWindow::getPlanListItem(QString planId)
{
    for (int i=0; i < ui->planList->count(); i++)
    {
        QListWidgetItem* item = ui->planList->item(i);
        QVariant data = item->data(Qt::UserRole);
        if (data.toString() == planId)
        {
            return item;
        }
    }

    return nullptr;
}

void MainWindow::onImportUserInfoBtn()
{
    // todo by yejinlong, onImportUserInfoBtn
}

void MainWindow::onAddPlanBtn()
{
    PlanDialog planDlg(PlanItem(), this);
    planDlg.show();
    if (planDlg.exec() == QDialog::Accepted)
    {
        PlanManager::getInstance()->m_plans.append(planDlg.getPlanItem());
        PlanManager::getInstance()->save();
        addPlanListItemCtrl(planDlg.getPlanItem());
    }
}

void MainWindow::onEditPlanBtn(QString planId)
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(planId);
    if (plan == nullptr)
    {
        return;
    }

    PlanDialog planDlg(*plan, this);
    planDlg.show();
    if (planDlg.exec() == QDialog::Accepted)
    {
        *plan = planDlg.getPlanItem();
        PlanManager::getInstance()->save();
        updatePlanListItemCtrl(plan->m_id);
    }
}

void MainWindow::onDeletePlanBtn(QString planId)
{
    PlanManager::getInstance()->deletePlan(planId);

    QListWidgetItem* item = getPlanListItem(planId);
    if (item)
    {
        ui->planList->removeItemWidget(item);
        delete item;
    }
}

void MainWindow::onRunPlanBtn(QString planId)
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(planId);
    if (plan == nullptr)
    {
        return;
    }

    plan->m_status = PLAN_STATUS_ADD_CART;
    updatePlanListItemCtrl(planId);
}

void MainWindow::onStopPlanBtn(QString planId)
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(planId);
    if (plan == nullptr)
    {
        return;
    }

    plan->m_status = PLAN_STATUS_STOPPING;
    updatePlanListItemCtrl(planId);
}

void MainWindow::addLog(QString log)
{
    qInfo(log.toStdString().c_str());
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString currentTimeString = currentDateTime.toString("[MM-dd hh:mm:ss] ");
    QString line = currentTimeString + log;
    ui->logEdit->append(line);
}
