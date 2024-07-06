#ifndef PLANMANAGER_H
#define PLANMANAGER_H

#include "datamodel.h"

class PlanManager
{    
protected:
    PlanManager();

public:
    static PlanManager* getInstance();

public:
    void save();

    PlanItem* getPlanById(QString id);

    void deletePlan(QString id);

private:
    void load();

public:
    // 计划列表
    QVector<PlanItem> m_plans;
};

#endif // PLANMANAGER_H
