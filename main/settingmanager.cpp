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
    loadRecommend();
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
    m_addCartUseProxy = root["addcart_use_proxy"].toBool();
    m_enableDebug = root["enable_debug"].toBool();
    m_proxyRegion = root["proxy_region"].toString();
    m_queryGoodInterval = root["query_good_interval"].toInt();
    m_stopStep = root["stop_step"].toInt();
    if (root.contains("use_cache_addcart"))
    {
        m_useCacheAddCartResult = root["use_cache_addcart"].toBool();
    }
    m_enableSelectFulfillmentDatetime = root["enable_select_fulfillment_datetime"].toBool();

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

void SettingManager::loadRecommend()
{
    std::wstring strConfFilePath = CImPath::GetConfPath() + L"recommend.json";
    QFile file(QString::fromStdWString(strConfFilePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR(L"failed to open the recommend configure file : %s", strConfFilePath.c_str());
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    QJsonObject root = jsonDocument.object();

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


bool SettingManager::importRecommends(QString recommendFilePath)
{
    QFile file(recommendFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QVector<RecommendedItem> recommendedItems;
    QTextStream in(&file);
    while (!in.atEnd())
    {
        QString line = in.readLine();
        int firstPos = line.indexOf("///");
        if (firstPos == -1)
        {
            continue;
        }

        int secondPos = line.indexOf("/A///", firstPos+3);
        if (secondPos == -1)
        {
            continue;
        }

        RecommendedItem recommendedItem;
        recommendedItem.m_name = line.mid(0, firstPos);
        int skuidBeginPos = firstPos+3;
        recommendedItem.m_skuid = line.mid(skuidBeginPos, secondPos-skuidBeginPos);
        recommendedItems.append(recommendedItem);
    }

    file.close();

    if (recommendedItems.size() == 0)
    {
        return false;
    }

    QJsonObject root;
    QJsonArray itemsJson;
    for (const auto& item : recommendedItems)
    {
        QJsonObject itemJson;
        itemJson["code"] = item.m_skuid;
        itemJson["name"] = item.m_name;
        itemsJson.append(itemJson);
    }
    root["recommended_item"] = itemsJson;

    QJsonDocument jsonDocument(root);
    QByteArray jsonData = jsonDocument.toJson(QJsonDocument::Indented);
    std::wstring recommendConfFilePath = CImPath::GetConfPath() + L"recommend.json";
    QFile recommendFile(QString::fromStdWString(recommendConfFilePath));
    if (!recommendFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical("failed to save the recommend setting");
        return false;
    }
    recommendFile.write(jsonData);
    recommendFile.close();

    m_recommendedItems = recommendedItems;
    return true;
}
