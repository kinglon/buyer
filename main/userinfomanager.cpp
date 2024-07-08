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
QString columnLastName = QString::fromWCharArray(L"姓");
QString columnFirstName = QString::fromWCharArray(L"名");
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

UserInfoManager::UserInfoManager()
{
    std::wstring excelFilePath = CImPath::GetConfPath() + L"用户资料.xlsx";
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

    std::wstring internalFilePath = CImPath::GetConfPath() + L"用户资料.xlsx";
    DeleteFile(internalFilePath.c_str());
    if (!CopyFile(excelFilePath.toStdWString().c_str(), internalFilePath.c_str(), TRUE))
    {
        return false;
    }

    // todo by yejinlong, 对敏感信息进行加密后更新到表格

    m_users = users;
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
                user.m_expiredData = value;
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
        users.append(user);
    }

    return true;
}
