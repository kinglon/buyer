﻿#include "planrunner.h"
#include "planmanager.h"
#include <QDateTime>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include "settingmanager.h"
#include <QFile>
#include "Utility/ImPath.h"
#include "Utility/ImCharset.h"
#include "Utility/LogMacro.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "userinfomanager.h"
#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxchart.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"
#include <QDesktopServices>
#include <QDir>

using namespace QXlsx;

PlanRunner::PlanRunner(QString planId, QObject *parent)
    : QObject{parent},
      m_planId(planId)
{

}

bool PlanRunner::start()
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        return false;
    }
    m_planName = plan->m_name;

    // 重建计划数据目录
    m_planDataPath = QString::fromStdWString(CImPath::GetDataPath()) + m_planName;
    QDir folderDir(m_planDataPath);
    if (folderDir.exists())
    {
        if (!folderDir.removeRecursively())
        {
            printLog(QString::fromWCharArray(L"无法删除数据目录，购买结果表格文件可能被打开"));
            return false;
        }
    }
    CreateDirectory(m_planDataPath.toStdWString().c_str(), nullptr);

    // 获取本地IP列表
    m_localIps = getLocalIps();
    qInfo("the number of the local ips is %d", m_localIps.size());
    if (m_localIps.size() == 0)
    {
        printLog(QString::fromWCharArray(L"获取本地IP列表失败"));
        return false;
    }

    if (!launchAddCartRunner(plan))
    {
        return false;
    }

    PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_ADD_CART);
    emit planStatusChange(m_planId);
    return true;
}

void PlanRunner::stop()
{
    m_requestStop = true;
    if (m_goodsChecker)
    {
        m_goodsChecker->requestStop();
    }

    for (auto& buyer : m_goodsBuyers)
    {
        buyer->requestStop();
    }
}

void PlanRunner::printLog(const QString& content)
{
    QString contentWithPrefix = "["+m_planName+"] "+content;
    emit log(contentWithPrefix);
}

QVector<QString> PlanRunner::getLocalIps()
{
    QVector<QString> ipAddresses;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& networkInterface : interfaces)
    {
        // Skip loopback and inactive interfaces
        if (networkInterface.flags().testFlag(QNetworkInterface::IsLoopBack) || !networkInterface.flags().testFlag(QNetworkInterface::IsUp))
        {
            continue;
        }

        QList<QNetworkAddressEntry> addressEntries = networkInterface.addressEntries();
        for (const QNetworkAddressEntry& entry : addressEntries)
        {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
            {
                ipAddresses.append(entry.ip().toString());
            }
        }
    }

    return ipAddresses;
}

bool PlanRunner::createAddCardRunnerParamFile(PlanItem* plan, QString paramFilePath)
{
    QJsonObject root;
    root["use_proxy"] = SettingManager::getInstance()->m_useProxy;
    root["proxy_region"] = SettingManager::getInstance()->m_proxyRegion;
    root["thread_num"] = plan->m_addCartThreadCount;

    PhoneModel* phoneModel = SettingManager::getInstance()->getPhoneModelByCode(plan->m_phoneCode);
    if (phoneModel == nullptr)
    {
        printLog(QString::fromWCharArray(L"skuid不存在"));
        return false;
    }

    QJsonObject phoneModelJson;
    phoneModelJson["model"] = phoneModel->m_model;
    phoneModelJson["phone_code"] = phoneModel->m_code;
    root["phone_model"] = phoneModelJson;

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
        printLog(QString::fromWCharArray(L"用户资料不够"));
        return false;
    }

    QJsonArray usersJson;
    for (int i=0; i<plan->m_count; i++)
    {
        QJsonObject userJson;
        userJson["account"] = UserInfoManager::getInstance()->m_users[begin+i].m_accountName;
        userJson["password"] = UserInfoManager::getInstance()->m_users[begin+i].m_password;
        usersJson.append(userJson);
    }
    root["user"] = usersJson;

    QJsonDocument jsonDocument(root);
    QByteArray jsonData = jsonDocument.toJson(QJsonDocument::Indented);
    QFile file(paramFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical("failed to save the param json file");
        return false;
    }
    file.write(jsonData);
    file.close();

    return true;
}

bool PlanRunner::launchAddCartRunner(PlanItem* plan)
{
    // 创建参数文件
    QString argsFilePath = m_planDataPath + "\\python_args.json";
    if (!createAddCardRunnerParamFile(plan, argsFilePath))
    {
        return false;
    }

    // 启动python程序，如果调试就跳过这个步骤
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;;
    si.wShowWindow = SW_HIDE;

    std::wstring strScriptPath = CImPath::GetSoftInstallPath() + L"python\\addcart.py";
    wchar_t command[MAX_PATH];
    _snwprintf_s(command, MAX_PATH, L"python.exe \"%s\" \"%s\" ", strScriptPath.c_str(), m_planDataPath.toStdWString().c_str());
    if (!CreateProcess(NULL, (LPWSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        qCritical("failed to create python process, error is %d", GetLastError());
        return false;
    }

    CloseHandle(pi.hThread);
    HANDLE processHandle = pi.hProcess;

    // 定时检查Python进程状态
    QTimer* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, [this, timer, processHandle]() {
        // 如果外部要求退出，就结束
        if (m_requestStop)
        {
            timer->stop();
            timer->deleteLater();
            TerminateProcess(processHandle, 1);
            CloseHandle(processHandle);
            emit runFinish(m_planId, false);
            return;
        }

        // 查询上货状态
        if (!queryAddCartRunnerStatus())
        {
            return;
        }

        // python程序已经运行结束，不需要等待
        timer->stop();
        timer->deleteLater();
        CloseHandle(processHandle);

        if (!loadAddCartResult())
        {
            emit runFinish(m_planId, false);
            return;
        }

        printLog(QString::fromWCharArray(L"上号成功"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_QUERY);
        emit planStatusChange(m_planId);
        launchGoodsChecker();
    });
    timer->start();

    return true;
}

bool PlanRunner::queryAddCartRunnerStatus()
{
    std::wstring planDataFilePath = m_planDataPath.toStdWString() + L"\\add_cart_progress.json";
    QFile file(QString::fromStdWString(planDataFilePath.c_str()));
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }    
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        return false;
    }

    QJsonObject root = jsonDocument.object();
    bool finish = false;
    QString finishMessage;
    int totalCount = 0;
    int successCount = 0;
    int failCount = 0;
    if (root.contains("total"))
    {
        totalCount = root["total"].toInt();
    }
    if (root.contains("success_count"))
    {
        successCount = root["success_count"].toInt();
    }
    if (root.contains("fail_count"))
    {
        failCount = root["fail_count"].toInt();
    }
    if (root.contains("finish"))
    {
        finish = root["finish"].toBool();
    }
    if (root.contains("finish_message"))
    {
        finishMessage = root["finish_message"].toString();
    }

    printLog(QString::fromWCharArray(L"上号中：%1/%2").arg(successCount+failCount, totalCount));
    if (!finish)
    {
        return false;
    }

    if (!finishMessage.isEmpty())
    {
        printLog(finishMessage);
    }

    printLog(QString::fromWCharArray(L"上号完成，总共%1，成功%2，失败%3").arg(totalCount, successCount, failCount));
    if (failCount > 0)
    {
        printLog(QString::fromWCharArray(L"上号失败的账号保存在：上号失败账号.txt"));
    }

    return true;
}

bool PlanRunner::loadAddCartResult()
{
    std::wstring planDataFilePath = m_planDataPath.toStdWString() + L"\\add_cart_result.json";
    QFile file(QString::fromStdWString(planDataFilePath.c_str()));
    if (!file.open(QIODevice::ReadOnly))
    {
        printLog(QString::fromWCharArray(L"上号结果文件加载失败"));
        return false;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
    if (jsonDocument.isNull() || jsonDocument.isEmpty())
    {
        printLog(QString::fromWCharArray(L"上号结果文件解析失败"));
        return false;
    }

    QJsonObject root = jsonDocument.object();
    for (auto buyParamJson : root["buy_param"].toArray())
    {
        BuyParam buyParam;
        buyParam.m_appStoreHost = buyParamJson.toObject()["appstore_host"].toString();
        buyParam.m_xAosStk = buyParamJson.toObject()["x_aos_stk"].toString();

        QJsonObject cookieJson = buyParamJson.toObject()["cookies"].toObject();
        for (auto& cookieKey : cookieJson.keys())
        {
            buyParam.m_cookies[cookieKey] = cookieJson[cookieKey].toString();
        }

        QString accountId = buyParamJson.toObject()["account"].toString();
        UserItem user = UserInfoManager::getInstance()->getUserByAccount(accountId);
        if (user.m_accountName.isEmpty())
        {
            printLog(QString::fromWCharArray(L"找不到账号信息：%1").arg(accountId));
            return false;
        }
        buyParam.m_user = user;
        m_buyParams.append(buyParam);
    }

    if (m_buyParams.size() == 0)
    {
        printLog(QString::fromWCharArray(L"上号成功个数为0"));
        return false;
    }

    return true;
}

void PlanRunner::launchGoodsChecker()
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        return;
    }

    if (plan->m_enableFixTimeBuy)
    {
        // 定时购买，开定时器等时间
        int fixBuyTime = plan->m_fixBuyTime;
        QVector<ShopItem>* shops = new QVector<ShopItem>();
        for (const auto& shopId : plan->m_buyingShops)
        {
            for (const auto& shop : SettingManager::getInstance()->m_shops)
            {
                if (shop.m_name == shopId)
                {
                    shops->append(shop);
                    break;
                }
            }
        }

        QTimer* timer = new QTimer(this);
        timer->setInterval(20);
        connect(timer, &QTimer::timeout, [this, timer, fixBuyTime, shops]() {
            // 如果外部要求退出，就结束
            if (m_requestStop)
            {
                delete shops;
                timer->stop();
                timer->deleteLater();
                emit runFinish(m_planId, false);
                return;
            }

            QDateTime now = QDateTime::currentDateTime();
            if (now.time().secsTo(QTime(0,0)) < fixBuyTime)
            {
                return;
            }

            timer->stop();
            timer->deleteLater();

            onGoodsCheckFinish(shops);
        });
        timer->start();
    }
    else
    {
        // 实时查询
        m_goodsChecker = new GoodsAvailabilityChecker();
        m_goodsChecker->setPhoneCode(plan->m_phoneCode);

        QVector<ShopItem> queryShops;
        for (const auto& shopId : plan->m_buyingShops)
        {
            for (const auto& shop : SettingManager::getInstance()->m_shops)
            {
                if (shop.m_name == shopId)
                {
                    queryShops.append(shop);
                    break;
                }
            }
        }
        m_goodsChecker->setShops(queryShops);

        connect(m_goodsChecker, &GoodsAvailabilityChecker::checkFinish, this, &PlanRunner::onGoodsCheckFinish);
        connect(m_goodsChecker, &GoodsAvailabilityChecker::printLog, this, &PlanRunner::printLog);
        connect(m_goodsChecker, &GoodsAvailabilityChecker::finished, m_goodsChecker, &QObject::deleteLater);
        m_goodsChecker->start();
    }
}

void PlanRunner::onGoodsCheckFinish(QVector<ShopItem>* shops)
{
    m_goodsChecker = nullptr;
    if (m_requestStop)
    {
        delete shops;
        emit runFinish(m_planId, false);
        return;
    }

    if (shops && !shops->empty())
    {
        printLog(QString::fromWCharArray(L"查询到有货"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_BUYING);
        emit planStatusChange(m_planId);

        for (int i=0; i<m_buyParams.size(); i++)
        {
            m_buyParams[i].m_buyingShop = (*shops)[i%(*shops).size()];
        }

        launchGoodsBuyer();
    }
    else
    {
        printLog(QString::fromWCharArray(L"店铺没货"));
        emit runFinish(m_planId, false);
    }

    delete shops;
}

void PlanRunner::launchGoodsBuyer()
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        return;
    }

    if (plan->m_count != m_buyParams.size())
    {
        qCritical("count is wrong");
        return;
    }

    m_goodsBuyers.clear();
    int countPerThread = plan->m_count / plan->m_threadCount;
    for (int i=0; i<plan->m_threadCount; i++)
    {        
        int realCount = countPerThread;
        if (i == plan->m_threadCount - 1)
        {
            realCount += plan->m_count % plan->m_threadCount;
        }

        if (realCount == 0)
        {
            continue;
        }

        qint64 now = GetTickCount64();
        QVector<QString> localIps;
        QVector<BuyParam> buyParams;
        for (int j=0; j<realCount; j++)
        {
            int index = i*countPerThread+j;
            localIps.push_back(m_localIps[index % m_localIps.size()]);
            m_buyParams[index].m_beginBuyTime = now;
            buyParams.push_back(m_buyParams[index]);
        }

        GoodsBuyer* buyer = new GoodsBuyer();
        buyer->setLocalIps(localIps);
        buyer->setParams(buyParams);
        connect(buyer, &GoodsBuyer::buyFinish, this, &PlanRunner::onGoodsBuyFinish);
        connect(buyer, &GoodsBuyer::printLog, this, &PlanRunner::printLog);
        connect(buyer, &GoodsBuyer::finished, buyer, &QObject::deleteLater);
        buyer->start();
        m_goodsBuyers.append(buyer);
    }
}

void PlanRunner::onGoodsBuyFinish(GoodsBuyer* buyer, QVector<BuyResult>* buyResults)
{
    if (buyResults)
    {
        m_buyResults.append(*buyResults);
        delete buyResults;
        buyResults = nullptr;
    }

    m_goodsBuyers.removeOne(buyer);
    if (m_goodsBuyers.empty())
    {
        finishPlan();
    }
}

bool PlanRunner::saveBuyingResult(const QVector<BuyResult>& buyResults)
{
    std::wstring srcExcelFilePath = CImPath::GetConfPath() + L"\\购买结果.xlsx";
    std::wstring destExcelFilePath = m_planDataPath.toStdWString() + L"\\购买结果.xlsx";
    DeleteFile(destExcelFilePath.c_str());
    if (!CopyFile(srcExcelFilePath.c_str(), destExcelFilePath.c_str(), TRUE))
    {
        qCritical("failed to copy the excel file of buying result");
        return false;
    }

    // 从第2行开始写
    QString qdestExcelFilePath = QString::fromStdWString(destExcelFilePath);
    Document xlsx(qdestExcelFilePath);
    if (!xlsx.load())
    {
        qCritical("failed to load the buying result excel file");
        return false;
    }

    int row = 2;
    for (const auto& buyResult : buyResults)
    {
        xlsx.write(row, 1, buyResult.m_account);
        xlsx.write(row, 2, buyResult.m_orderNo);        
        xlsx.write(row, 3, buyResult.getStepName());
        QString takeTimes;
        for (const auto& takeTime : buyResult.m_takeTimes)
        {
            if (!takeTimes.isEmpty())
            {
                takeTimes += ",";
            }
            takeTimes += QString::number(takeTime);
        }
        xlsx.write(row, 4, takeTimes);
        xlsx.write(row, 5, buyResult.m_localIp);
        row++;
    }

    if (!xlsx.save())
    {
        qCritical("failed to save the buying result excel file");
        return false;
    }

    QString log = QString::fromWCharArray(L"购买结果保存到：%1").arg(qdestExcelFilePath);
    printLog(log);

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_planDataPath));

    return true;
}

void PlanRunner::finishPlan()
{
    saveBuyingResult(m_buyResults);
    emit runFinish(m_planId, true);
}
