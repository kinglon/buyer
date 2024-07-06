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
    m_networkRequestInterval = root["network_request_interval"].toInt();

    QJsonArray shops = root["shop"].toArray();
    for (auto shop : shops)
    {
        QJsonObject shopJson = shop.toObject();
        ShopItem shopItem;
        shopItem.m_name = shopJson["name"].toString();
        shopItem.m_postalCode = shopJson["postal"].toString();
        m_shops.append(shopItem);
    }

    QJsonArray phoneModels = root["phone_model"].toArray();
    for (auto phoneModel : phoneModels)
    {
        QJsonObject phoneModelJson = phoneModel.toObject();
        PhoneModel phoneModelItem;
        phoneModelItem.m_code = phoneModelJson["code"].toString();
        phoneModelItem.m_model = phoneModelJson["model"].toString();
        phoneModelItem.m_name = phoneModelJson["name"].toString();
        m_phoneModels.append(phoneModelItem);
    }
}
