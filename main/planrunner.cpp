#include "planrunner.h"
#include "planmanager.h"
#include <QDateTime>
#include <QTimer>
#include "Utility/ImPath.h"
#include "settingmanager.h"

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
    // todo by yejinlong, 获取结果
    bool success = false;
    if (success)
    {
        printLog(QString::fromWCharArray(L"加货完成"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_QUERY);
        emit planStatusChange(m_planId);
        launchGoodsChecker();
    }
    else
    {
        printLog(QString::fromWCharArray(L"加货失败"));
        emit runFinish(m_planId);
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
        emit printLog(QString::fromWCharArray(L"查询到有货"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_BUYING);
        emit planStatusChange(m_planId);

        m_shops = shops;
        launchGoodsBuyer();
    }
    else
    {
        emit printLog(QString::fromWCharArray(L"店铺没货"));
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

    m_goodsBuyers.clear();
    for (int i=0; i<plan->m_threadCount; i++)
    {
        GoodsBuyer* buyer = new GoodsBuyer();
        connect(buyer, &GoodsBuyer::buyFinish, this, &PlanRunner::onGoodsBuyFinish);
        connect(buyer, &GoodsBuyer::printLog, this, &PlanRunner::printLog);
        connect(buyer, &GoodsBuyer::finished, buyer, &QObject::deleteLater);
        buyer->start();
        m_goodsBuyers.append(buyer);
    }
}

void PlanRunner::onGoodsBuyFinish(GoodsBuyer* buyer)
{
    m_goodsBuyers.removeOne(buyer);
    if (m_goodsBuyers.empty())
    {
        finishPlan();
    }
}

void PlanRunner::finishPlan()
{
    // todo by yejinlong, 保存结果
    // 日志提示结果在哪个目录下
    // 自动打开这个目录

    emit runFinish(m_planId);
}
