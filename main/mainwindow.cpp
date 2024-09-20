#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "plandialog.h"
#include "planmanager.h"
#include "planitemwidget.h"
#include <QDateTime>
#include <QFileDialog>
#include "planrunner.h"
#include "userinfomanager.h"
#include "uiutil.h"
#include "fixtimebuyer.h"
#include "Utility/ImPath.h"
#include "settingmanager.h"
#include "proxymanager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

    initCtrls();

    ProxyManager::getInstance()->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initCtrls()
{
    ui->planList->setItemDelegate(new MyItemDelegate(ui->planList));
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

    connect(ui->importRecommendBtn, &QPushButton::clicked, [this]() {
        onImportRecommendBtn();
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

bool MainWindow::createPlanDataDirectory(QString planName)
{
    // 重建计划数据目录
    QString planDataPath = QString::fromStdWString(CImPath::GetDataPath()) + planName;
    if (!SettingManager::getInstance()->m_useCacheAddCartResult)
    {
        QDir folderDir(planDataPath);
        if (folderDir.exists())
        {
            if (!UiUtil::showTipV2(QString::fromWCharArray(L"原来购买数据将被清空，是否继续？")))
            {
                return false;
            }

            if (!folderDir.removeRecursively())
            {
                addLog(QString::fromWCharArray(L"无法删除数据目录，购买结果表格文件可能被打开"));
                return false;
            }
        }
        CreateDirectory(planDataPath.toStdWString().c_str(), nullptr);
    }

    return true;
}

void MainWindow::onImportUserInfoBtn()
{
    // Create a QFileDialog object
    QFileDialog fileDialog;

    // Set the dialog's title
    fileDialog.setWindowTitle(QString::fromWCharArray(L"选择表格文件"));

    // Set the dialog's filters
    fileDialog.setNameFilters(QStringList() << "Excel files (*.xlsx *.xls)");

    // Open the dialog and get the selected files
    if (fileDialog.exec() == QDialog::Accepted)
    {
        QStringList selectedFiles = fileDialog.selectedFiles();
        if (selectedFiles.size() > 0)
        {
            if (!UserInfoManager::getInstance()->importUserInfo(selectedFiles[0]))
            {
                UiUtil::showTip(QString::fromWCharArray(L"加载用户资料失败"));
            }
            else
            {
                int count = UserInfoManager::getInstance()->m_users.size();
                addLog(QString::fromWCharArray(L"加载用户资料成功，共%1条").arg(count));
            }
        }
    }
}

void MainWindow::onImportRecommendBtn()
{
    // Create a QFileDialog object
    QFileDialog fileDialog;

    // Set the dialog's title
    fileDialog.setWindowTitle(QString::fromWCharArray(L"选择配件列表文件"));

    // Set the dialog's filters
    fileDialog.setNameFilters(QStringList() << "Text files (*.txt)");

    // Open the dialog and get the selected files
    if (fileDialog.exec() == QDialog::Accepted)
    {
        QStringList selectedFiles = fileDialog.selectedFiles();
        if (selectedFiles.size() > 0)
        {
            if (!SettingManager::getInstance()->importRecommends(selectedFiles[0]))
            {
                UiUtil::showTip(QString::fromWCharArray(L"导入配件列表失败"));
            }
            else
            {
                int count = SettingManager::getInstance()->m_recommendedItems.size();
                addLog(QString::fromWCharArray(L"导入配件列表成功，共%1条").arg(count));
            }
        }
    }
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
    ProxyServer proxyServer = ProxyManager::getInstance()->getProxyServer();
    if (proxyServer.m_ip.isEmpty())
    {
        UiUtil::showTip(QString::fromWCharArray(L"正在获取代理IP，请稍后"));
        return;
    }

    PlanItem* plan = PlanManager::getInstance()->getPlanById(planId);
    if (plan == nullptr)
    {
        return;
    }

    PlanRunner* planRunner = m_planRunners[planId];
    if (planRunner)
    {
        qCritical("the plan has been running");
        return;
    }

    if (!createPlanDataDirectory(plan->m_name))
    {
        return;
    }

    planRunner = new PlanRunner(planId, this);
    connect(planRunner, &PlanRunner::log, this, &MainWindow::addLog);
    connect(planRunner, &PlanRunner::planStatusChange, [this] (QString planId) {
        updatePlanListItemCtrl(planId);
    });
    connect(planRunner, &PlanRunner::runFinish, [this] (QString planId, bool success) {
        PlanManager::getInstance()->setPlanStatus(planId, success?PLAN_STATUS_STOPPING:PLAN_STATUS_INIT);
        updatePlanListItemCtrl(planId);
        PlanRunner* planRunner = m_planRunners[planId];
        m_planRunners[planId] = nullptr;
        if (planRunner)
        {
            planRunner->deleteLater();
        }
    });

    if (planRunner->start())
    {
        m_planRunners[planId] = planRunner;
    }
    else
    {
        planRunner->deleteLater();
    }
}

void MainWindow::onStopPlanBtn(QString planId)
{
    PlanRunner* planRunner = m_planRunners[planId];
    if (planRunner == nullptr)
    {
        qCritical("the plan has not been running");
        return;
    }

    planRunner->stop();
}

void MainWindow::addLog(QString log)
{
    static int lineCount = 0;
    if (lineCount >= 1000)
    {
        ui->logEdit->clear();
        lineCount = 0;
    }
    lineCount++;

    qInfo(log.toStdString().c_str());
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString currentTimeString = currentDateTime.toString("[MM-dd hh:mm:ss] ");
    QString line = currentTimeString + log;
    ui->logEdit->append(line);
}
