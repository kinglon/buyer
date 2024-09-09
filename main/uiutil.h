#ifndef UIUTIL_H
#define UIUTIL_H

#include <QString>

class UiUtil
{
public:
    static void showTip(QString tip);

    // 显示提示，让用户选择，继续返回true
    static bool showTipV2(QString tip);
};

#endif // UIUTIL_H
