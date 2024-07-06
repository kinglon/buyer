#ifndef MULTISELECTIONDIALOG_H
#define MULTISELECTIONDIALOG_H

#include <QDialog>

namespace Ui {
class MultiSelectionDialog;
}

class MultiSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiSelectionDialog(const QVector<QString>& items,
                                  const QVector<QString>& selectionItems,
                                  QWidget *parent = nullptr);
    ~MultiSelectionDialog();

private:
    void initCtrls();

private:
    void onOkBtn();

public:
    QVector<QString> m_items;

    QVector<QString> m_selectionItems;

private:
    Ui::MultiSelectionDialog *ui;
};

#endif // MULTISELECTIONDIALOG_H
