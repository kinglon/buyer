#pragma once
#include <string>

//职责：程序崩溃后生成最小化转储文件
struct _EXCEPTION_POINTERS;
class CDumpUtil
{
public:
    /**
    @name 设置转储文件保存路径
    */
    static void SetDumpFilePath(const wchar_t* szDumpFilePath);

    /**
    @name 设置是否要生成最小化转储文件
    */
    static void Enable(bool bEnable);

private:
    static long __stdcall UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* pExceptionInfo);
    static void DisableSetUnhandledExceptionFilter();

private:
    static std::wstring m_strDumpFilePath;
};
