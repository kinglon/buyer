#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QString>
#include <QVector>

#define GUDONG_DATA_SIZE  9
#define NIANBAO_DATA_SIZE 7

class DataModel
{
public:
    // 公司名字
    QString m_companyName;

    // 股东数据
    QVector<QString> m_gudongData;

    // 年报数据
    QVector<QString> m_nianbaoData;
};

#endif // DATAMODEL_H
