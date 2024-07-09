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

    QString m_proxyRegion = "jp";

    // 网络请求间隔秒数
    int m_networkRequestInterval = 2;

    // 店铺列表
    QVector<ShopItem> m_shops;

    // 机型列表
    QVector<PhoneModel> m_phoneModels;
};
