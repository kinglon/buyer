#include "userinfomanager.h"
#include "Utility/ImPath.h"
#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxchart.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"

using namespace QXlsx;

#define COLUMN_SIZE 15

// 列名
QString columnAccount = QString::fromWCharArray(L"id账号");
QString columnPassword = QString::fromWCharArray(L"id密码");
QString columnFirstName = QString::fromWCharArray(L"姓");
QString columnLastName = QString::fromWCharArray(L"名");
QString columnTelephone = QString::fromWCharArray(L"电话");
QString columnEmail = QString::fromWCharArray(L"邮箱");
QString columnCreditCardNo = QString::fromWCharArray(L"信用卡卡号");
QString columnExpiredDate = QString::fromWCharArray(L"有效期");
QString columnCvv = QString::fromWCharArray(L"CVV");
QString columnPostal = QString::fromWCharArray(L"邮编");
QString columnState = QString::fromWCharArray(L"地址（省）");
QString columnCity = QString::fromWCharArray(L"地址（市）");
QString columnStreet1 = QString::fromWCharArray(L"地址（区）");
QString columnStreet2 = QString::fromWCharArray(L"地址（具体门牌号）");
QString columnGiftNo = QString::fromWCharArray(L"礼品卡号");

// 用户资料模板文件名
#define USER_TEMPLATE_FILE_NAME     L"用户资料模板.xlsx"

// 当前用户资料文件名
#define CURRENT_USER_FILE_NAME     L"用户资料.xlsx"

UserInfoManager::UserInfoManager()
{
    std::wstring excelFilePath = CImPath::GetConfPath() + CURRENT_USER_FILE_NAME;
    QVector<QString> headers;
    loadUserInfo(QString::fromStdWString(excelFilePath), true, headers, m_users);
}

UserInfoManager* UserInfoManager::getInstance()
{
    static UserInfoManager* instance = new UserInfoManager();
    return instance;
}

UserItem UserInfoManager::getUserByAccount(QString account)
{
    for (const auto& user : m_users)
    {
        if (account == user.m_accountName)
        {
            return user;
        }
    }

    return UserItem();
}

bool UserInfoManager::importUserInfo(QString excelFilePath)
{
    QVector<QString> headers;
    QVector<UserItem> users;
    if (!loadUserInfo(excelFilePath, false, headers, users))
    {
        return false;
    }

    if (!save(users))
    {
        return false;
    }

    m_users = users;
    return true;
}

bool UserInfoManager::deleteUsers(const QVector<QString>& accountNames)
{
    for (const auto& accountName : accountNames)
    {
        for (auto it=m_users.begin(); it!=m_users.end(); it++)
        {
            if (it->m_accountName == accountName)
            {
                m_users.erase(it);
                break;
            }
        }
    }

    if (!save(m_users))
    {
        return false;
    }

    return true;
}

bool UserInfoManager::loadUserInfo(QString excelFilePath, bool , QVector<QString>& headers, QVector<UserItem>& users)
{
    Document xlsx(excelFilePath);
    if (!xlsx.load())
    {
        return false;
    }

    // 获取头部
    for (int column=1; column <= COLUMN_SIZE; column++)
    {
        Cell* cell = xlsx.cellAt(1, column);
        if (cell)
        {
            headers.push_back(cell->readValue().toString());
        }
        else
        {
            headers.push_back("");
        }
    }

    // 获取数据
    CellRange range = xlsx.dimension();
    for (int row=2; row <= range.lastRow(); row++)
    {
        UserItem user;
        for (int column=1; column <= COLUMN_SIZE; column++)
        {
            Cell* cell = xlsx.cellAt(row, column);
            if (cell == nullptr)
            {
                break;
            }

            QString columnName = headers[column-1];
            QString value = cell->readValue().toString();
            if (columnName == columnAccount)
            {
                user.m_accountName = value;
            }
            else if (columnName == columnCity)
            {
                user.m_city = value;
            }
            if (columnName == columnCreditCardNo)
            {
                user.m_creditCardNo = value;
            }
            if (columnName == columnCvv)
            {
                user.m_cvv = value;
            }
            if (columnName == columnEmail)
            {
                user.m_email = value;
            }
            if (columnName == columnExpiredDate)
            {
                user.m_expiredDate = value;
            }
            if (columnName == columnFirstName)
            {
                user.m_firstName = value;
            }
            if (columnName == columnLastName)
            {
                user.m_lastName = value;
            }
            if (columnName == columnGiftNo)
            {
                user.m_giftCardNo = value;
            }
            if (columnName == columnPassword)
            {
                user.m_password = value;
            }
            if (columnName == columnPostal)
            {
                user.m_postalCode = value;
            }
            if (columnName == columnState)
            {
                user.m_state = value;
            }
            if (columnName == columnStreet1)
            {
                user.m_street = value;
            }
            if (columnName == columnStreet2)
            {
                user.m_street2 = value;
            }
            if (columnName == columnTelephone)
            {
                user.m_telephone = value;
            }
        }

        if (user.isValid())
        {
            users.append(user);
        }
    }

    return true;
}

bool UserInfoManager::save(const QVector<UserItem>& users)
{
    // 拷贝默认模板
    std::wstring srcExcelFilePath = CImPath::GetConfPath() + USER_TEMPLATE_FILE_NAME;
    std::wstring destExcelFilePath = CImPath::GetDataPath() + CURRENT_USER_FILE_NAME;
    ::DeleteFile(destExcelFilePath.c_str());
    if (!::CopyFile(srcExcelFilePath.c_str(), destExcelFilePath.c_str(), TRUE))
    {
        qCritical("failed to copy the default excel file");
        return false;
    }

    Document xlsx(QString::fromStdWString(destExcelFilePath));
    if (!xlsx.load())
    {
        qCritical("failed to load the default excel file");
        return false;
    }

    // 从第2行开始写
    int row = 2;
    for (const auto& user : users)
    {
        int column = 1;
        xlsx.write(row, column++, user.m_accountName);
        xlsx.write(row, column++, user.m_password);
        xlsx.write(row, column++, user.m_firstName);
        xlsx.write(row, column++, user.m_lastName);
        xlsx.write(row, column++, user.m_telephone);
        xlsx.write(row, column++, user.m_email);
        xlsx.write(row, column++, user.m_creditCardNo);
        xlsx.write(row, column++, user.m_expiredDate);
        xlsx.write(row, column++, user.m_cvv);
        xlsx.write(row, column++, user.m_postalCode);
        xlsx.write(row, column++, user.m_state);
        xlsx.write(row, column++, user.m_city);
        xlsx.write(row, column++, user.m_street);
        xlsx.write(row, column++, user.m_street2);
        row++;
    }

    if (!xlsx.save())
    {
        qCritical("failed to save the user excel file");
        return false;
    }

    return true;
}
