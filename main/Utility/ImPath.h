#pragma once

#include <string>
#include <shtypes.h>

#pragma comment(lib, "shlwapi.lib")
#pragma warning(disable:4215)
class CImPath
{
public:
    /**
    @name 获取软件安装路径，执行文件所在的路径
    */
    static std::wstring GetSoftInstallPath();

    /**
    @name 获取应用数据路径，以斜杠结尾
    */
	static std::wstring GetDataPath();

    /**
    @name 获取缓存数据路径，以斜杠结尾
    */
	static std::wstring GetCachePath();

    /**
    @name 获取系统本地临时文件夹路径%AppData%\\Local
    */
	static std::wstring GetLocalAppDataPath();

	/**
	@name 获取系统本地临时文件夹路径%AppData%\\Roaming
	*/
	static std::wstring GetAppDataRoamingPath();

    /**
    @name 获取配置路径，以斜杠结尾
    */
    static std::wstring GetConfPath();

    /**
    @name 获取日志路径，以斜杠结尾
    */
    static std::wstring GetLogPath();

    /**
    @name 获取dump路径，以斜杠结尾
    */
    static std::wstring GetDumpPath();

private:	
	static std::wstring GetSystemTempPath();
};
