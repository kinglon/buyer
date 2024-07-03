#pragma once

#include <QString>
#include <QVector>

class CSettingManager
{
protected:
	CSettingManager();

public:
	static CSettingManager* GetInstance();

private:
	void Load();

public:
    int m_nLogLevel = 2;  // info level
};
