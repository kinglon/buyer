#include "debugdialog.h"
#include "ui_debugdialog.h"

DebugDialog::DebugDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DebugDialog)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::WindowModal);

    connect(ui->mockHaveGoodsBtn, &QPushButton::clicked, [this]() {
        emit mockHaveGoods();
    });
}

DebugDialog::~DebugDialog()
{
    delete ui;
}
