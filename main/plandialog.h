#ifndef PLANDIALOG_H
#define PLANDIALOG_H

#include <QDialog>
#include "datamodel.h"

namespace Ui {
class PlanDialog;
}

class PlanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlanDialog(const PlanItem& planItem, QWidget *parent = nullptr);
    ~PlanDialog();

public:
    const PlanItem& getPlanItem() { return m_planItem;}

private:
    void initCtrls();

    void updateCtrls();

    void updateShopCtrls();

private:
    void onOkBtn();

    void onSelectShopBtn();

private:
    Ui::PlanDialog *ui;

    PlanItem m_planItem;
};

#endif // PLANDIALOG_H
