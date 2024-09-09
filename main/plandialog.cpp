#include "plandialog.h"
#include <QThread>
#include <QUuid>
#include <QTimeZone>
#include "ui_plandialog.h"
#include "settingmanager.h"
#include "uiutil.h"
#include "multiselectiondialog.h"

PlanDialog::PlanDialog(const PlanItem& planItem, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlanDialog),
    m_planItem(planItem)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::WindowModal);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

    initCtrls();
}

PlanDialog::~PlanDialog()
{
    delete ui;
}

void PlanDialog::initCtrls()
{
    ui->skuidComboBox->clear();
    for (const auto& phoneModel : SettingManager::getInstance()->m_phoneModels)
    {
        ui->skuidComboBox->addItem(phoneModel.m_name, QVariant(phoneModel.m_code));
    }

    ui->paymentComboBox->clear();
    ui->paymentComboBox->addItem(PlanItem::getPaymentName(PAYMENT_CREDIT_CARD), QVariant(PAYMENT_CREDIT_CARD));
    ui->paymentComboBox->addItem(PlanItem::getPaymentName(PAYMENT_GIFT_CARD), QVariant(PAYMENT_GIFT_CARD));

    int threadCount = QThread::idealThreadCount();
    QString threadCountString = QString::fromWCharArray(L"总线程数%1").arg(threadCount);
    ui->totalThreadCountLabel->setText(threadCountString);    

    connect(ui->cancelBtn, &QPushButton::clicked, [this] () {
        close();
    });
    connect(ui->okBtn, &QPushButton::clicked, [this] () {
        onOkBtn();
    });
    connect(ui->selectShopBtn, &QPushButton::clicked, [this] () {
        onSelectShopBtn();
    });

    updateCtrls();
}

void PlanDialog::updateCtrls()
{
   ui->nameEdit->setText(m_planItem.m_name);

   ui->skuidComboBox->setCurrentIndex(-1);
   if (!m_planItem.m_phoneCode.isEmpty())
   {
       for (int i = 0; i < ui->skuidComboBox->count(); i++)
       {
           QVariant itemData = ui->skuidComboBox->itemData(i);
           if (itemData.toString() == m_planItem.m_phoneCode)
           {
               ui->skuidComboBox->setCurrentIndex(i);
               break;
           }
       }
   }

   ui->paymentComboBox->setCurrentIndex(-1);
   if (m_planItem.m_payment != PAYMENT_NONE)
   {
       for (int i = 0; i < ui->paymentComboBox->count(); i++)
       {
           QVariant itemData = ui->paymentComboBox->itemData(i);
           if (itemData.toInt() == m_planItem.m_payment)
           {
               ui->paymentComboBox->setCurrentIndex(i);
               break;
           }
       }
   }

   ui->countEdit->setText(QString::number(m_planItem.m_count));

   ui->addCartThreadCountEdit->setText(QString::number(m_planItem.m_addCartThreadCount));

   ui->threadCountEdit->setText(QString::number(m_planItem.m_threadCount));

   if (m_planItem.m_enableFixTimeBuy)
   {
       ui->fixTimeCheckBox->setChecked(true);
       QDateTime dateTime = QDateTime::fromSecsSinceEpoch(m_planItem.m_fixBuyTime);
       QTimeZone desiredTimeZone("Asia/Shanghai");
       dateTime.setTimeZone(desiredTimeZone);
       ui->fixDateTimeEdit->setDateTime(dateTime);
   }
   else
   {
       ui->fixTimeCheckBox->setChecked(false);
       QDateTime now = QDateTime::currentDateTime();
       QTimeZone desiredTimeZone("Asia/Shanghai");
       now.setTimeZone(desiredTimeZone);
       ui->fixDateTimeEdit->setDateTime(now);
   }

   updateShopCtrls();
}

void PlanDialog::updateShopCtrls()
{
    QString shops;
    for (const auto& shop : m_planItem.m_buyingShops)
    {
        if (!shops.isEmpty())
        {
            shops += ',';
        }
        shops += shop;
    }
    ui->shopIdEdit->setPlainText(shops);
}

void PlanDialog::onOkBtn()
{
    m_planItem.m_name = ui->nameEdit->text();
    if (m_planItem.m_name.isEmpty())
    {
        UiUtil::showTip(QString::fromWCharArray(L"名字不能为空"));
        return;
    }

    m_planItem.m_phoneCode = "";
    int skuidSelIndex = ui->skuidComboBox->currentIndex();
    if (skuidSelIndex >= 0)
    {
        QVariant itemData = ui->skuidComboBox->itemData(skuidSelIndex);
        m_planItem.m_phoneCode = itemData.toString();
    }
    if (m_planItem.m_phoneCode.isEmpty())
    {
        UiUtil::showTip(QString::fromWCharArray(L"skuid不能为空"));
        return;
    }

    m_planItem.m_payment = PAYMENT_NONE;
    int paymentSelIndex = ui->paymentComboBox->currentIndex();
    if (paymentSelIndex >= 0)
    {
        QVariant itemData = ui->paymentComboBox->itemData(paymentSelIndex);
        m_planItem.m_payment = itemData.toInt();
    }
    if (m_planItem.m_payment == PAYMENT_NONE)
    {
        UiUtil::showTip(QString::fromWCharArray(L"付款方式未选择"));
        return;
    }

    int count = ui->countEdit->text().toInt();
    if (count <= 0)
    {
        UiUtil::showTip(QString::fromWCharArray(L"购买数量未填写"));
        return;
    }
    m_planItem.m_count = count;

    int threadCount = ui->threadCountEdit->text().toInt();
    if (threadCount <= 0)
    {
        UiUtil::showTip(QString::fromWCharArray(L"购买线程数未填写"));
        return;
    }
    m_planItem.m_threadCount = threadCount;

    int addCartThreadCount = ui->addCartThreadCountEdit->text().toInt();
    if (addCartThreadCount <= 0)
    {
        UiUtil::showTip(QString::fromWCharArray(L"上号线程数未填写"));
        return;
    }
    m_planItem.m_addCartThreadCount = addCartThreadCount;

    m_planItem.m_enableFixTimeBuy = ui->fixTimeCheckBox->isChecked();
    if (m_planItem.m_enableFixTimeBuy)
    {
        m_planItem.m_fixBuyTime = ui->fixDateTimeEdit->dateTime().toSecsSinceEpoch();
    }

    if (m_planItem.m_buyingShops.empty())
    {
        UiUtil::showTip(QString::fromWCharArray(L"未选择购买店铺"));
        return;
    }

    if (m_planItem.m_id.isEmpty())
    {
        QUuid guid = QUuid::createUuid();
        QString guidString = guid.toString();
        m_planItem.m_id = guidString;
    }

    done(Accepted);
}

void PlanDialog::onSelectShopBtn()
{
    QVector<QString> items;
    for (const auto& shop : SettingManager::getInstance()->m_shops)
    {
        items.push_back(shop.m_name);
    }

    MultiSelectionDialog dialog(items, m_planItem.m_buyingShops, this);
    dialog.show();
    if (dialog.exec() == Accepted)
    {
        m_planItem.m_buyingShops = dialog.m_selectionItems;
        updateShopCtrls();
    }
}
