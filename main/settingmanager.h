#pragma once

#include <QString>
#include <QVector>
#include "datamodel.h"

class SettingManager
{
protected:
    SettingManager();

public:
    static SettingManager* getInstance();

public:
    PhoneModel* getPhoneModelByCode(QString code);

private:
    void load();

public:
    int m_logLevel = 2;  // info level

    bool m_useProxy = true;

    bool m_enableDebug = false;

    QString m_proxyRegion = "jp";

    // 查询是否有货间隔，毫秒
    int m_queryGoodInterval = 100;

    // 停止步骤，开发调试使用
    int m_stopStep = 100;

    // 店铺列表
    QVector<ShopItem> m_shops;

    // 机型列表
    QVector<PhoneModel> m_phoneModels;
};
