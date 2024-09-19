#include "settingmanager.h"
#include <QFile>
#include "Utility/ImPath.h"
#include "Utility/ImCharset.h"
#include "Utility/LogMacro.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SettingManager::SettingManager()
{
    load();
}

SettingManager* SettingManager::getInstance()
{
    static SettingManager* pInstance = new SettingManager();
	return pInstance;
}

PhoneModel* SettingManager::getPhoneModelByCode(QString code)
{
    for (auto& phoneModel : m_phoneModels)
    {
        if (phoneModel.m_code == code)
        {
            return &phoneModel;
        }
    }

    return nullptr;
}

void SettingManager::load()
{
    std::wstring strConfFilePath = CImPath::GetConfPath() + L"configs.json";    
    QFile file(QString::fromStdWString(strConfFilePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR(L"failed to open the basic configure file : %s", strConfFilePath.c_str());
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    QJsonObject root = jsonDocument.object();

    m_logLevel = root["log_level"].toInt();
    m_useProxy = root["use_proxy"].toBool();
    m_enableDebug = root["enable_debug"].toBool();
    m_proxyRegion = root["proxy_region"].toString();
    m_queryGoodInterval = root["query_good_interval"].toInt();
    m_stopStep = root["stop_step"].toInt();
    if (root.contains("use_cache_addcart"))
    {
        m_useCacheAddCartResult = root["use_cache_addcart"].toBool();
    }

    QJsonArray shops = root["shop"].toArray();
    for (auto shop : shops)
    {
        QJsonObject shopJson = shop.toObject();
        ShopItem shopItem;
        shopItem.m_name = shopJson["name"].toString();
        shopItem.m_postalCode = shopJson["postal"].toString();
        shopItem.m_storeNumber = shopJson["id"].toString();
        m_shops.append(shopItem);
    }

    QJsonObject phoneModels = root["phone_model"].toObject();
    for (const auto& phoneModel : phoneModels.keys())
    {        
        QJsonArray phoneModelJsonArray = phoneModels[phoneModel].toArray();
        for (const auto& phoneModelJson : phoneModelJsonArray)
        {
            PhoneModel phoneModelItem;
            phoneModelItem.m_model = phoneModel;
            phoneModelItem.m_code = phoneModelJson.toObject()["code"].toString();
            phoneModelItem.m_name = phoneModelJson.toObject()["name"].toString();
            m_phoneModels.append(phoneModelItem);
        }
    }

    QJsonArray recommendedItems = root["recommended_item"].toArray();
    for (auto item : recommendedItems)
    {
        QJsonObject itemJson = item.toObject();
        RecommendedItem recommendedItem;
        recommendedItem.m_name = itemJson["name"].toString();
        recommendedItem.m_skuid = itemJson["code"].toString();
        m_recommendedItems.append(recommendedItem);
    }
}
