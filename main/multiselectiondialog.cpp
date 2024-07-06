#include "multiselectiondialog.h"
#include "ui_multiselectiondialog.h"

MultiSelectionDialog::MultiSelectionDialog(const QVector<QString>& items,
                                           const QVector<QString>& selectionItems,
                                           QWidget *parent) :
    QDialog(parent),
    m_items(items),
    m_selectionItems(selectionItems),
    ui(new Ui::MultiSelectionDialog)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::WindowModal);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

    initCtrls();
}

MultiSelectionDialog::~MultiSelectionDialog()
{
    delete ui;
}

void MultiSelectionDialog::initCtrls()
{
    for (const auto& item : m_items)
    {
        ui->listWidget->addItem(item);
    }

    for (const auto& selectionItem : m_selectionItems)
    {
        QList<QListWidgetItem*> items = ui->listWidget->findItems(selectionItem, Qt::MatchExactly);
        foreach (QListWidgetItem* item, items)
        {
            item->setSelected(true);
        }
    }

    connect(ui->cancelBtn, &QPushButton::clicked, [this] () {
        close();
    });
    connect(ui->okBtn, &QPushButton::clicked, [this] () {
        onOkBtn();
    });
}

void MultiSelectionDialog::onOkBtn()
{
    m_selectionItems.clear();
    QList<QListWidgetItem*> selectedItems = ui->listWidget->selectedItems();
    foreach (QListWidgetItem* item, selectedItems)
    {
        m_selectionItems.push_back(item->text());
    }

    done(Accepted);
}
