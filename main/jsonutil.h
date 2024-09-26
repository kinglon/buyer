#ifndef JSONUTIL_H
#define JSONUTIL_H

#include <QJsonObject>
#include <QJsonArray>

class JsonUtil
{
public:
    JsonUtil();

public:
    // 按keys获取指定的对象，每个key对应都是对象
    static bool findObject(QJsonObject& root, const QVector<QString>& keys, QJsonObject& result);
};

#endif // JSONUTIL_H
