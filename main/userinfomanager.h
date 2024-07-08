#ifndef USERINFOMANAGER_H
#define USERINFOMANAGER_H

#include "datamodel.h"

class UserInfoManager
{
public:
    UserInfoManager();

    static UserInfoManager* getInstance();

    UserItem getUserByAccount(QString account);

    // 从外部导入用户资料
    bool importUserInfo(QString excelFilePath);

private:
    // 从表格加载用户资料，encrypt 标志表格的敏感信息是否加密
    bool loadUserInfo(QString excelFilePath, bool encrypt, QVector<QString>& headers, QVector<UserItem>& users);

public:
    QVector<UserItem> m_users;
};

#endif // USERINFOMANAGER_H
