#include "uiutil.h"
#include <QMessageBox>

void UiUtil::showTip(QString tip)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(QString::fromWCharArray(L"提示"));
    msgBox.setText(tip);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

bool UiUtil::showTipV2(QString tip)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(QString::fromWCharArray(L"提示"));
    msgBox.setText(tip);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    int result = msgBox.exec();
    if (result == QMessageBox::Ok)
    {
        return true;
    }

    return false;
}
