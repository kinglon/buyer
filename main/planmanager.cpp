#include "planmanager.h"
#include <QFile>
#include "Utility/ImPath.h"
#include "Utility/ImCharset.h"
#include "Utility/LogMacro.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

PlanManager::PlanManager()
{
    load();
}


PlanManager* PlanManager::getInstance()
{
    static PlanManager* instance = new PlanManager();
    return instance;
}

void PlanManager::save()
{
    QJsonObject root;
    QJsonArray plansJson;
    for (const auto& plan : m_plans)
    {
        QJsonObject planJson;
        planJson["id"] = plan.m_id;
        planJson["name"] = plan.m_name;
        planJson["phone_code"] = plan.m_phoneCode;
        planJson["count"] = plan.m_count;
        planJson["thread_count"] = plan.m_threadCount;
        planJson["payment"] = plan.m_payment;

        QJsonArray shopsJson;
        for (const auto& shop : plan.m_buyingShops)
        {
            shopsJson.append(shop);
        }
        planJson["shop"] = shopsJson;
        plansJson.append(planJson);
    }
    root["plan"] = plansJson;

    QJsonDocument jsonDocument(root);
    QByteArray jsonData = jsonDocument.toJson(QJsonDocument::Indented);
    std::wstring strConfFilePath = CImPath::GetConfPath() + L"plan.json";
    QFile file(QString::fromStdWString(strConfFilePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical("failed to save the plan setting");
        return;
    }
    file.write(jsonData);
    file.close();
}

void PlanManager::load()
{
    std::wstring strConfFilePath = CImPath::GetConfPath() + L"plan.json";
    QFile file(QString::fromStdWString(strConfFilePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    QJsonObject root = jsonDocument.object();

    QJsonArray plans = root["plan"].toArray();
    for (auto plan : plans)
    {
        QJsonObject planJson = plan.toObject();
        PlanItem planItem;
        planItem.m_id = planJson["id"].toString();
        planItem.m_name = planJson["name"].toString();
        planItem.m_phoneCode = planJson["phone_code"].toString();
        planItem.m_count = planJson["count"].toInt();
        planItem.m_threadCount = planJson["thread_count"].toInt();
        planItem.m_payment = planJson["payment"].toInt();

        planItem.m_buyingShops.clear();
        QJsonArray shops = planJson["shop"].toArray();
        for (auto shop : shops)
        {
            planItem.m_buyingShops.append(shop.toString());
        }
        m_plans.append(planItem);
    }
}

PlanItem* PlanManager::getPlanById(QString id)
{
    for (auto& plan : PlanManager::getInstance()->m_plans)
    {
        if (plan.m_id == id)
        {
            return &plan;
        }
    }

    return nullptr;
}

void PlanManager::deletePlan(QString id)
{
    for (auto it = m_plans.begin(); it != m_plans.end(); it++)
    {
        if (it->m_id == id)
        {
            m_plans.erase(it);
            break;
        }
    }

    save();
}

void PlanManager::setPlanStatus(QString id, int status)
{
    for (auto it = m_plans.begin(); it != m_plans.end(); it++)
    {
        if (it->m_id == id)
        {
            it->m_status = status;
            break;
        }
    }
}
