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

    m_localIps = getLocalIps();
    qInfo("the number of the local ips is %d", m_localIps.size());
    if (m_localIps.size() == 0)
    {
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

bool PlanRunner::launchAddCartRunner(PlanItem* plan)
{
    // todo by yejinlong, 创建python_args.json文件

    // 获取参数文件的路径
    QDateTime dateTime = QDateTime::currentDateTime();
    QString currentTime = dateTime.toString(QString::fromWCharArray(L"yyyyMMdd_hhmm"));
    QString argsFolderName = plan->m_name + "_" + currentTime;
    std::wstring argsFolderPath = CImPath::GetDataPath() + argsFolderName.toStdWString();

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
    si.dwFlags = STARTF_USESHOWWINDOW;;
    si.wShowWindow = SW_HIDE;

    std::wstring strScriptPath = CImPath::GetSoftInstallPath() + L"addcart.py";
    wchar_t command[MAX_PATH];
    _snwprintf_s(command, MAX_PATH, L"python.exe \"%s\" \"%s\" ", strScriptPath.c_str(), argsFolderPath.c_str());
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
            CloseHandle(processHandle);
            emit runFinish(m_planId);
            return;
        }

        // python程序还在运行，继续等待
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processHandle, &exitCode) && exitCode == STILL_ACTIVE)
        {
            return;
        }

        // python程序已经运行结束，不需要等待
        timer->stop();
        timer->deleteLater();
        CloseHandle(processHandle);

        // 查询上货状态
        queryAddCartRunnerStatus();
    });
    timer->start();

    return true;
}

void PlanRunner::queryAddCartRunnerStatus()
{
    std::wstring planDataFilePath = CImPath::GetDataPath() + m_planName.toStdWString() + L"\\add_cart_result.json";
    QFile file(QString::fromStdWString(planDataFilePath.c_str()));
    if (!file.open(QIODevice::ReadOnly))
    {
        printLog(QString::fromWCharArray(L"加货失败，无法打开结果文件"));
        emit runFinish(m_planId);
        return;
    }
    else
    {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData);
        if (jsonDocument.isNull() || jsonDocument.isEmpty())
        {
            printLog(QString::fromWCharArray(L"加货失败，无法解析结果文件"));
            emit runFinish(m_planId);
            return;
        }

        QJsonObject root = jsonDocument.object();
        bool success = root["success"].toBool();
        if (!success)
        {
            printLog(QString::fromWCharArray(L"加货失败，%1").arg(root["message"].toString()));
            emit runFinish(m_planId);
            return;
        }

        for (auto buyParamJson : root["buy_param"].toArray())
        {
            BuyParam buyParam;
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
                emit runFinish(m_planId);
                return;
            }
            buyParam.m_user = user;
            m_buyParams.append(buyParam);
        }

        printLog(QString::fromWCharArray(L"加货完成"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_QUERY);
        emit planStatusChange(m_planId);
        launchGoodsChecker();
    }
}

void PlanRunner::launchGoodsChecker()
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        return;
    }

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

void PlanRunner::onGoodsCheckFinish(QVector<ShopItem> shops)
{
    m_goodsChecker = nullptr;
    if (m_requestStop)
    {
        emit runFinish(m_planId);
    }

    if (!shops.empty())
    {
        printLog(QString::fromWCharArray(L"查询到有货"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_BUYING);
        emit planStatusChange(m_planId);

        for (int i=0; i<m_buyParams.size(); i++)
        {
            m_buyParams[i].m_buyingShop = shops[i%shops.size()];
        }

        launchGoodsBuyer();
    }
    else
    {
        printLog(QString::fromWCharArray(L"店铺没货"));
        emit runFinish(m_planId);
    }
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

        QVector<QString> localIps;
        QVector<BuyParam> buyParams;
        for (int j=0; j<realCount; j++)
        {
            int index = i*countPerThread+j;
            localIps.push_back(m_localIps[index % m_localIps.size()]);
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

void PlanRunner::onGoodsBuyFinish(GoodsBuyer* buyer, QVector<BuyResult> buyResults)
{
    m_buyResults.append(buyResults);

    m_goodsBuyers.removeOne(buyer);
    if (m_goodsBuyers.empty())
    {
        finishPlan();
    }
}

bool PlanRunner::saveBuyingResult(const QVector<BuyResult>& buyResults)
{
    std::wstring srcExcelFilePath = CImPath::GetConfPath() + L"\\购买结果.xlsx";
    std::wstring destExcelSavePath = CImPath::GetDataPath() + m_planName.toStdWString();
    std::wstring destExcelFilePath = destExcelSavePath + L"\\购买结果.xlsx";
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
        if (buyResult.m_orderNo.isEmpty())
        {
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
        }
        row++;
    }

    if (!xlsx.save())
    {
        qCritical("failed to save the buying result excel file");
        return false;
    }

    QString log = QString::fromWCharArray(L"购买结果保存到：%1").arg(qdestExcelFilePath);
    printLog(log);

    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(destExcelSavePath)));

    return true;
}

void PlanRunner::finishPlan()
{
    saveBuyingResult(m_buyResults);
    emit runFinish(m_planId);
}
