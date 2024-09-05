#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QString>
#include <QVector>

// 支付方式
#define PAYMENT_NONE            0
#define PAYMENT_CREDIT_CARD     1
#define PAYMENT_GIFT_CARD       2

// 计划状态
#define PLAN_STATUS_INIT                1   // 初始化
#define PLAN_STATUS_ADD_CART            2   // 上号中
#define PLAN_STATUS_QUERY               3   // 上号成功
#define PLAN_STATUS_BUYING              4   // 正在购物
#define PLAN_STATUS_STOPPING            5   // 购物结束

class UserItem
{
public:
    // 账号
    QString m_accountName;

    // 密码
    QString m_password;

    // 姓
    QString m_firstName;

    // 名
    QString m_lastName;

    // 电话
    QString m_telephone;

    // 邮箱
    QString m_email;

    // 信用卡卡号
    QString m_creditCardNo;

    // 过期时间
    QString m_expiredDate;

    // CVV
    QString m_cvv;

    // 邮编
    QString m_postalCode;

    // 地址（省）
    QString m_state;

    // 地址（市)
    QString m_city;

    // 地址（区）
    QString m_street;

    // 地址（街道）
    QString m_street2;

    // 礼品卡号
    QString m_giftCardNo;

public:
    bool isValid()
    {
        if (m_accountName.isEmpty() || m_password.isEmpty() || m_firstName.isEmpty() || m_lastName.isEmpty()
                || m_telephone.isEmpty() || m_email.isEmpty() || m_creditCardNo.isEmpty()
                || m_expiredDate.isEmpty() || m_cvv.isEmpty() || m_postalCode.isEmpty()
                || m_state.isEmpty() || m_city.isEmpty() || m_street.isEmpty())
        {
            return false;
        }
        else
        {
            return true;
        }
    }
};

class ShopItem
{
public:
    // 店铺名字
    QString m_name;

    // 店铺邮编
    QString m_postalCode;

    // 店铺ID
    QString m_storeNumber;
};

class PlanItem
{
public:
    // 计划ID
    QString m_id;

    // 计划名字
    QString m_name;

    // 购买机型，机型码
    QString m_phoneCode;

    // 购买数量
    int m_count = 1;

    // 上号线程数
    int m_addCartThreadCount = 1;

    // 购买线程数
    int m_threadCount = 1;

    // 是否开启定时购买
    bool m_enableFixTimeBuy = false;

    // 定时购买的时间，秒数
    int m_fixBuyTime = 0;

    // 购买店铺列表
    QVector<QString> m_buyingShops;

    // 支付方式
    int m_payment = PAYMENT_CREDIT_CARD;

    // 状态
    int m_status = PLAN_STATUS_INIT;

public:
    static QString getStatusName(int status)
    {
        if (status == PLAN_STATUS_INIT)
        {
            return QString::fromWCharArray(L"");
        }
        else if (status == PLAN_STATUS_ADD_CART)
        {
            return QString::fromWCharArray(L"上号中");
        }
        else if (status == PLAN_STATUS_QUERY)
        {
            return QString::fromWCharArray(L"上号成功");
        }
        else if (status == PLAN_STATUS_BUYING)
        {
            return QString::fromWCharArray(L"正在购物");
        }
        else if (status == PLAN_STATUS_STOPPING)
        {
            return QString::fromWCharArray(L"购物结束");
        }
        else
        {
            return "";
        }
    }

    static QString getPaymentName(int payment)
    {
        if (payment == PAYMENT_CREDIT_CARD)
        {
            return QString::fromWCharArray(L"信用卡支付");
        }
        else if (payment == PAYMENT_GIFT_CARD)
        {
            return QString::fromWCharArray(L"礼品卡支付");
        }
        else
        {
            return "";
        }
    }
};

// 机型
class PhoneModel
{
public:
    // 机型码，如 MTU93J
    QString m_code;

    // 名字，如 iphone 15 pro max 1T 天然钛
    QString m_name;

    // 型号，如 iphone-15-pro
    QString m_model;
};

#endif // DATAMODEL_H
