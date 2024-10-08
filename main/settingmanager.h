﻿#pragma once

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

    bool importRecommends(QString recommendFilePath);

private:
    void load();

    void loadRecommend();

public:
    int m_logLevel = 2;  // info level

    // 除上号外是否使用代理
    bool m_useProxy = false;

    // 上号是否使用代理
    bool m_addCartUseProxy = true;

    bool m_enableDebug = false;

    QString m_proxyRegion = "jp";

    // 使用缓存的上号结果
    bool m_useCacheAddCartResult = false;

    // 查询是否有货间隔，毫秒
    int m_queryGoodInterval = 100;

    // 停止步骤，开发调试使用
    int m_stopStep = 100;

    // 标志自提是否需要选择日期和时间
    bool m_enableSelectFulfillmentDatetime = false;

    // 店铺列表
    QVector<ShopItem> m_shops;

    // 机型列表
    QVector<PhoneModel> m_phoneModels;

    // 配件列表
    QVector<RecommendedItem> m_recommendedItems;
};
