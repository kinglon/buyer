#include "fixtimebuyer.h"
#include "planmanager.h"
#include "Utility/ImPath.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "settingmanager.h"
#include "userinfomanager.h"

FixTimeBuyer::FixTimeBuyer()
{

}


FixTimeBuyer* FixTimeBuyer::getInstance()
{
    static FixTimeBuyer* instance = new FixTimeBuyer();
    return instance;
}

bool FixTimeBuyer::isRunning()
{
    if (m_hProcess == NULL)
    {
        return false;
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode == STILL_ACTIVE)
    {
        return true;
    }

    CloseHandle(m_hProcess);
    m_hProcess = NULL;
    return false;
}

bool FixTimeBuyer::start(QString planId)
{
    if (isRunning())
    {
        return true;
    }

    PlanItem* plan = PlanManager::getInstance()->getPlanById(planId);
    if (plan == nullptr)
    {
        return false;
    }

    // 重新创建计划数据目录
    QString planDataPath = QString::fromStdWString(CImPath::GetDataPath()) + plan->m_name;
    QDir folderDir(planDataPath);
    if (folderDir.exists())
    {
        if (!folderDir.removeRecursively())
        {
            m_lastError = QString::fromWCharArray(L"无法删除数据目录，购买结果表格文件可能被打开");
            return false;
        }
    }
    CreateDirectory(planDataPath.toStdWString().c_str(), nullptr);

    // 创建参数文件
    QString argsFilePath = planDataPath + "\\python_args_buy.json";
    if (!createBuyerParamFile(plan, argsFilePath))
    {
        return false;
    }

    // 启动python程序
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    std::wstring strScriptPath = CImPath::GetSoftInstallPath() + L"python\\buy.py";
    wchar_t command[3*MAX_PATH];
    _snwprintf_s(command, 3*MAX_PATH, L"python.exe \"%s\" \"%s\" ", strScriptPath.c_str(), planDataPath.toStdWString().c_str());
    if (!CreateProcess(NULL, (LPWSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        qCritical("failed to create python process, error is %d", GetLastError());
        m_lastError = QString::fromWCharArray(L"启动python购买脚本失败，确认已安装python运行环境和依赖库");
        return false;
    }

    CloseHandle(pi.hThread);
    m_hProcess = pi.hProcess;
    return true;
}

bool FixTimeBuyer::createBuyerParamFile(PlanItem* plan, QString paramFilePath)
{
    QJsonObject root;
    root["proxy_region"] = SettingManager::getInstance()->m_proxyRegion;
    root["plan_id"] = plan->m_id;

    PhoneModel* phoneModel = SettingManager::getInstance()->getPhoneModelByCode(plan->m_phoneCode);
    if (phoneModel == nullptr)
    {
        m_lastError = QString::fromWCharArray(L"skuid不存在");
        return false;
    }

    QJsonObject phoneModelJson;
    phoneModelJson["model"] = phoneModel->m_model;
    phoneModelJson["code"] = phoneModel->m_code;
    phoneModelJson["name"] = phoneModel->m_name;
    root["phone"] = phoneModelJson;

    // 计算分配的用户范围
    int begin = 0;
    for (const auto& planItem : PlanManager::getInstance()->m_plans)
    {
        if (planItem.m_id != plan->m_id)
        {
            begin += planItem.m_count;
        }
        else
        {
            break;
        }
    }

    if (begin + plan->m_count > UserInfoManager::getInstance()->m_users.length())
    {
        m_lastError = QString::fromWCharArray(L"用户资料不够");
        return false;
    }

    QJsonArray usersJson;
    for (int i=0; i<plan->m_count; i++)
    {
        QJsonObject userJson;
        UserItem& user = UserInfoManager::getInstance()->m_users[begin+i];
        userJson["account"] = user.m_accountName;
        userJson["password"] = user.m_password;
        userJson["first_name"] = user.m_firstName;
        userJson["last_name"] = user.m_lastName;
        userJson["telephone"] = user.m_telephone;
        userJson["email"] = user.m_email;
        userJson["credit_card_no"] = user.m_creditCardNo;
        userJson["expired_date"] = user.m_expiredDate;
        userJson["cvv"] = user.m_cvv;
        userJson["postal_code"] = user.m_postalCode;
        userJson["state"] = user.m_state;
        userJson["city"] = user.m_city;
        userJson["street"] = user.m_street;
        userJson["street2"] = user.m_street2;
        usersJson.append(userJson);
    }
    root["user"] = usersJson;

    QJsonDocument jsonDocument(root);
    QByteArray jsonData = jsonDocument.toJson(QJsonDocument::Indented);
    QFile file(paramFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical("failed to save the param json file");
        m_lastError = QString::fromWCharArray(L"保存参数文件失败");
        return false;
    }
    file.write(jsonData);
    file.close();

    return true;
}
