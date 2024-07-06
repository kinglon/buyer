#include "planitemwidget.h"
#include "ui_planitemwidget.h"
#include "settingmanager.h"
#include "planmanager.h"

PlanItemWidget::PlanItemWidget(const QString& planId, QWidget *parent) :
    QWidget(parent),    
    ui(new Ui::PlanItemWidget),
    m_planId(planId)
{
    ui->setupUi(this);

    initCtrls();
    updateCtrls();
}

PlanItemWidget::~PlanItemWidget()
{
    delete ui;
}

void PlanItemWidget::initCtrls()
{
    connect(ui->editBtn, &QPushButton::clicked, [this]() {
        emit editPlan(m_planId);
    });

    connect(ui->deleteBtn, &QPushButton::clicked, [this]() {
        emit deletePlan(m_planId);
    });

    connect(ui->runBtn, &QPushButton::clicked, [this]() {
        if (isRunning())
        {
            emit stopPlan(m_planId);
        }
        else
        {
            emit runPlan(m_planId);
        }
    });
}

bool PlanItemWidget::isRunning()
{
    PlanItem* planItem = PlanManager::getInstance()->getPlanById(m_planId);
    if (planItem && planItem->m_status != PLAN_STATUS_TO_ADD_CART)
    {
        return true;
    }

    return false;
}

void PlanItemWidget::updateCtrls()
{
    PlanItem* planItem = PlanManager::getInstance()->getPlanById(m_planId);
    if (planItem == nullptr)
    {
        return;
    }

    ui->planNameLabel->setText(planItem->m_name);

    PhoneModel* phoneModel = SettingManager::getInstance()->getPhoneModelByCode(planItem->m_phoneCode);
    if (phoneModel)
    {
        ui->skuidLabel->setText(phoneModel->m_name);
    }

    ui->payLabel->setText(PlanItem::getPaymentName(planItem->m_payment));
    ui->countLabel->setText(QString::number(planItem->m_count));
    ui->statusLabel->setText(PlanItem::getStatusName(planItem->m_status));

    if (planItem->m_status == PLAN_STATUS_STOPPING)
    {
        ui->runBtn->setEnabled(false);
    }
    else
    {
        ui->runBtn->setEnabled(true);
    }

    if (!isRunning())
    {
        ui->runBtn->setText(QString::fromWCharArray(L"运行"));
    }
    else
    {
        ui->runBtn->setText(QString::fromWCharArray(L"停止"));
    }

    if (planItem->m_status == PLAN_STATUS_TO_ADD_CART)
    {
        ui->editBtn->setEnabled(true);
        ui->deleteBtn->setEnabled(true);
    }
    else
    {
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
    }
}
