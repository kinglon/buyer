#include "jsonutil.h"

JsonUtil::JsonUtil()
{

}

bool JsonUtil::findObject(QJsonObject& root, const QVector<QString>& keys, QJsonObject& result)
{
    QJsonObject parent = root;
    for (auto& key : keys)
    {
        if (!parent.contains(key))
        {
            return false;
        }
        parent = parent[key].toObject();
    }
    result = parent;
    return true;
}
