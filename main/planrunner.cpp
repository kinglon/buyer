#include "planrunner.h"
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
    m_planDataPath = QString::fromStdWString(CImPath::GetDataPath()) + m_planName;

    if (!launchAddCartRunner(plan))
    {
        printLog(QString::fromWCharArray(L"启动上号程序失败"));
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

    if (m_sessionUpdater)
    {
        m_sessionUpdater->requestStop();
        m_sessionUpdater = nullptr;
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

    root["recommended_item"] = plan->m_recommendedSkuid;

    // 随便选择一家店铺，真正下单的时候会改成有货的店铺
    if (plan->m_buyingShops.size() == 0)
    {
        printLog(QString::fromWCharArray(L"购买店铺未配置"));
        return false;
    }

    QString shopId;
    QString shopPostalCode;
    for (auto& shop : SettingManager::getInstance()->m_shops)
    {
        if (shop.m_name == plan->m_buyingShops[0])
        {
            shopId = shop.m_storeNumber;
            shopPostalCode = shop.m_postalCode;
            break;
        }
    }
    if (shopId.isEmpty())
    {
        printLog(QString::fromWCharArray(L"购买店铺未找到"));
        return false;
    }

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
        UserItem& userItem = UserInfoManager::getInstance()->m_users[begin+i];
        userJson["account"] = userItem.m_accountName;
        userJson["password"] = userItem.m_password;
        userJson["first_name"] = userItem.m_firstName;
        userJson["last_name"] = userItem.m_lastName;
        userJson["telephone"] = userItem.m_telephone;
        userJson["email"] = userItem.m_email;
        userJson["credit_card_no"] = userItem.m_creditCardNo;
        userJson["expired_date"] = userItem.m_expiredDate;
        userJson["cvv"] = userItem.m_cvv;
        userJson["postal_code"] = userItem.m_postalCode;
        userJson["state"] = userItem.m_state;
        userJson["city"] = userItem.m_city;
        userJson["street"] = userItem.m_street;
        userJson["street2"] = userItem.m_street2;
        if (plan->m_payment == PAYMENT_GIFT_CARD)
        {
            userJson["giftcard_no"] = userItem.m_giftCardNo;
        }
        else
        {
            userJson["giftcard_no"] = "";
        }
        userJson["store"] = shopId;
        userJson["store_postal_code"] = shopPostalCode;
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
    // 使用缓存上号数据，测试时使用
    if (SettingManager::getInstance()->m_useCacheAddCartResult)
    {
        if (!loadAddCartResult())
        {            
            return false;
        }

        printLog(QString::fromWCharArray(L"上号成功"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_QUERY);
        emit planStatusChange(m_planId);
        launchGoodsChecker();
        launchSessionUpdater();
        return true;
    }

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
    wchar_t command[3*MAX_PATH];
    _snwprintf_s(command, 3*MAX_PATH, L"python.exe \"%s\" \"%s\" ", strScriptPath.c_str(), m_planDataPath.toStdWString().c_str());
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
        launchSessionUpdater();
    });
    timer->start();

    return true;
}

bool PlanRunner::queryAddCartRunnerStatus()
{
    std::wstring planDataFilePath = m_planDataPath.toStdWString() + L"\\add_cart_progress.json";
    QFile file(QString::fromStdWString(planDataFilePath.c_str()));
    if (!file.exists())
    {
        printLog(QString::fromWCharArray(L"上号中"));
        return false;
    }

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

    printLog(QString::fromWCharArray(L"上号中：%1/%2")
             .arg(QString::number(successCount+failCount), QString::number(totalCount)));
    if (!finish)
    {
        return false;
    }

    if (!finishMessage.isEmpty())
    {
        printLog(finishMessage);
    }

    printLog(QString::fromWCharArray(L"上号完成，总共%1，成功%2，失败%3")
             .arg(QString::number(totalCount), QString::number(successCount), QString::number(failCount)));
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
        buyParam.m_proxyIp = buyParamJson.toObject()["proxy_ip"].toString();
        buyParam.m_proxyPort = buyParamJson.toObject()["proxy_port"].toInt();
        buyParam.m_localIp = buyParamJson.toObject()["local_ip"].toString();

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

    m_goodsChecker = new GoodsAvailabilityChecker();
    m_goodsChecker->setPhoneCode(plan->m_phoneCode);

    QVector<ShopItem> queryShops;
    for (const auto& shopName : plan->m_buyingShops)
    {
        for (const auto& shop : SettingManager::getInstance()->m_shops)
        {
            if (shop.m_name == shopName)
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

void PlanRunner::launchSessionUpdater()
{
    m_sessionUpdater = new SessionUpdater();
    m_sessionUpdater->setParams(m_buyParams);
    connect(m_sessionUpdater, &SessionUpdater::finished, m_sessionUpdater, &QObject::deleteLater);
    m_sessionUpdater->start();
}

void PlanRunner::onGoodsCheckFinish(QVector<ShopItem>* shops)
{
    if (m_sessionUpdater)
    {
        m_buyParams = m_sessionUpdater->getBuyParams();
        m_sessionUpdater->requestStop();
        m_sessionUpdater = nullptr;
    }
    m_goodsChecker = nullptr;
    if (m_requestStop)
    {
        delete shops;
        emit runFinish(m_planId, false);
        return;
    }

    if (shops && !shops->empty())
    {
        printLog(QString::fromWCharArray(L"正在购物"));
        PlanManager::getInstance()->setPlanStatus(m_planId, PLAN_STATUS_BUYING);
        emit planStatusChange(m_planId);

        for (int i=0; i<m_buyParams.size(); i++)
        {
            m_buyParams[i].m_buyingShop = (*shops)[i%(*shops).size()];
        }

        if (!launchGoodsBuyer())
        {
            printLog(QString::fromWCharArray(L"购物失败"));
            emit runFinish(m_planId, false);
        }
    }
    else
    {
        printLog(QString::fromWCharArray(L"店铺没货"));
        emit runFinish(m_planId, false);
    }

    delete shops;
}

bool PlanRunner::launchGoodsBuyer()
{
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        return false;
    }

    m_goodsBuyers.clear();
    int totalBuyCount = m_buyParams.size();
    int countPerThread = totalBuyCount / plan->m_threadCount;
    for (int i=0; i<plan->m_threadCount; i++)
    {        
        int realCount = countPerThread;
        if (i == plan->m_threadCount - 1)
        {
            realCount += totalBuyCount % plan->m_threadCount;
        }

        if (realCount == 0)
        {
            continue;
        }

        qint64 now = GetTickCount64();
        QVector<BuyParam> buyParams;
        for (int j=0; j<realCount; j++)
        {
            int index = i*countPerThread+j;
            m_buyParams[index].m_beginBuyTime = now;
            buyParams.push_back(m_buyParams[index]);
        }

        GoodsBuyer* buyer = new GoodsBuyer();
        buyer->setParams(buyParams);
        connect(buyer, &GoodsBuyer::buyFinish, this, &PlanRunner::onGoodsBuyFinish);
        connect(buyer, &GoodsBuyer::printLog, this, &PlanRunner::printLog);
        connect(buyer, &GoodsBuyer::finished, buyer, &QObject::deleteLater);
        buyer->start();
        m_goodsBuyers.append(buyer);
    }

    return true;
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
    PlanItem* plan = PlanManager::getInstance()->getPlanById(m_planId);
    if (plan == nullptr)
    {
        qCritical("failed to find the plan: %s", m_planId.toStdString().c_str());
        return false;
    }

    PhoneModel* phoneModel = SettingManager::getInstance()->getPhoneModelByCode(plan->m_phoneCode);
    if (phoneModel == nullptr)
    {
        qCritical("failed to find the phone model: %s", plan->m_phoneCode.toStdString().c_str());
        return false;
    }

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

    QString now = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss");
    int row = 2;
    for (const auto& buyResult : buyResults)
    {
        static QString successMsg = QString::fromWCharArray(L"购买成功");
        static QString failMsg = QString::fromWCharArray(L"购买失败");

        UserItem user = UserInfoManager::getInstance()->getUserByAccount(buyResult.m_account);

        int column = 1;
        xlsx.write(row, column++, ""); // Robot_id
        xlsx.write(row, column++, plan->m_name); // 任务名称
        xlsx.write(row, column++, plan->m_count); // 数量
        QString buyDateTime = buyResult.m_beginBuyDateTime.toString("yyyy/MM/dd hh:mm:ss");
        xlsx.write(row, column++, buyDateTime); // 开抢时间
        xlsx.write(row, column++, phoneModel->m_name); // iphonesku.spec_value
        xlsx.write(row, column++, buyResult.m_addCartProxy); // 代理IP
        xlsx.write(row, column++, buyResult.m_localIp); // 购买IP
        xlsx.write(row, column++, buyResult.m_success?successMsg:failMsg); // 购物状态
        xlsx.write(row, column++, buyResult.m_orderNo); // 订单号
        xlsx.write(row, column++, ""); // 订单状态
        QString orderLink;
        if (!buyResult.m_orderNo.isEmpty())
        {
            orderLink = QString("https://www.apple.com/xc/jp/vieworder/%1/%2").arg(buyResult.m_orderNo, user.m_email);
        }
        xlsx.write(row, column++, orderLink); // 下单链接
        xlsx.write(row, column++, "jp"); // 地区

        QString orderInfo;
        if (!user.m_accountName.isEmpty())
        {
            orderInfo = QString::fromWCharArray(L"电话：%1, 名字：%2， 地址：%3")
                    .arg(user.m_telephone, user.m_lastName+user.m_firstName,
                         user.m_state+user.m_city+user.m_street+user.m_street2);
        }
        xlsx.write(row, column++, orderInfo); // 下单信息
        xlsx.write(row, column++, buyResult.m_account); // 下单帐号
        xlsx.write(row, column++, buyResult.m_buyShopName); // 店铺
        xlsx.write(row, column++, now); // 更新时间
        xlsx.write(row, column++, user.m_email); // 邮箱
        xlsx.write(row, column++, user.m_password); // 密码
        xlsx.write(row, column++, buyResult.m_success?"":buyResult.m_failReason); // 失败原因

        QString takeTime;
        for (int i=0; i<buyResult.m_takeTimes.size(); i++)
        {
            if (!takeTime.isEmpty())
            {
                takeTime += ", ";
            }
            takeTime += buyResult.m_takeTimes[i];
        }
        xlsx.write(row, column++, takeTime); // 耗时
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

bool PlanRunner::deleteSuccessUsers(const QVector<BuyResult>& buyResults)
{
    QVector<QString> accountNames;
    for (auto& buyResult : buyResults)
    {
        if (!buyResult.m_orderNo.isEmpty())
        {
            accountNames.append(buyResult.m_account);
        }
    }

    if (accountNames.size() == 0)
    {
        return true;
    }

    if (!UserInfoManager::getInstance()->deleteUsers(accountNames))
    {
        emit printLog(QString::fromWCharArray(L"删除下单成功的用户资料失败"));
        return false;
    }
    else
    {
        QString message = QString::fromWCharArray(L"删除下单成功的用户资料%1条，现剩%2条").arg(
                    QString::number(accountNames.size()),
                    QString::number(UserInfoManager::getInstance()->m_users.size()));
        emit printLog(message);
        return true;
    }
}

void PlanRunner::finishPlan()
{    
    saveBuyingResult(m_buyResults);
    deleteSuccessUsers(m_buyResults);
    emit runFinish(m_planId, true);
}
