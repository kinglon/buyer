#include <Windows.h>
#include <dbghelp.h>
#include "DumpUtil.h"
#include <time.h>
#include "ImCharset.h"
#include "LogMacro.h"

using namespace std;

// Based on dbghelp.h
typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess,
    DWORD dwPid,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

wstring CDumpUtil::m_strDumpFilePath;

void CDumpUtil::SetDumpFilePath(const wchar_t* szDumpFilePath)
{
    m_strDumpFilePath = szDumpFilePath;
}

long __stdcall CDumpUtil::UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* pExceptionInfo)
{
    LOG_DEBUG(L"catch an unhandle exception");

    wstring strDumpFile = m_strDumpFilePath;
    time_t currentTime;
    time(&currentTime);
    wchar_t szFileName[100];
    memset(szFileName, 0, sizeof(szFileName));
    struct tm tmCurrent;
    if (localtime_s(&tmCurrent, &currentTime) != 0)
    {
        strDumpFile += L"minidump.dmp";
    }
    else
    {
        tmCurrent.tm_year = tmCurrent.tm_year + 1900;
        tmCurrent.tm_mon = tmCurrent.tm_mon + 1;
        _snwprintf_s(szFileName, ARRAYSIZE(szFileName), L"minidump_%04d%02d%02d_%02d%02d%02d.dmp",
            tmCurrent.tm_year, tmCurrent.tm_mon, tmCurrent.tm_mday, tmCurrent.tm_hour, tmCurrent.tm_min, tmCurrent.tm_sec);
        strDumpFile += szFileName;
    }

    LOG_DEBUG(L"dump file path = %s", strDumpFile.c_str());
    HANDLE hFile = CreateFile(strDumpFile.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR(L"failed to create dump file %s", strDumpFile.c_str());
        exit(1);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = pExceptionInfo;
    exceptionInfo.ClientPointers = TRUE;
    
    //先加载系统的dbghelp.dll，如果失败再加载本地的dll
    HMODULE hDbghelpDll = LoadLibrary(L"DBGHELP.DLL");
    if (hDbghelpDll == NULL)
    {
        LOG_DEBUG(L"failed to load system dbghelp dll");
        hDbghelpDll = LoadLibrary(L"DBGHELP2.DLL");
        if (hDbghelpDll == NULL)
        {
            LOG_ERROR(L"failed to load local dbghelp dll, error=%d", GetLastError());
            CloseHandle(hFile);
            exit(1);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    MINIDUMPWRITEDUMP pMiniDumpWriteDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDbghelpDll, "MiniDumpWriteDump");
    if (pMiniDumpWriteDump)
    {        
        if (!pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &exceptionInfo, nullptr, nullptr))
        {
            LOG_ERROR(L"failed to write mini dump to file, error=%d", GetLastError());
        }
    }
    else
    {
        LOG_ERROR(L"failed to get MiniDumpWriteDump function address");
    }
    
    LOG_DEBUG(L"exit process because of exception");
    CloseHandle(hFile);
    FreeLibrary(hDbghelpDll);
    exit(1);
    return EXCEPTION_EXECUTE_HANDLER;
}

void CDumpUtil::DisableSetUnhandledExceptionFilter()
{
    void *addr = (void*)GetProcAddress(LoadLibrary(L"kernel32.dll"), "SetUnhandledExceptionFilter");
    if (addr)
    {
        //LOG_DEBUG(LOG_TAG, "disable SetUnhandledExceptionFilter function to be called");
        unsigned char code[16];
        int size = 0;
        code[size++] = 0x33;
        code[size++] = 0xC0;
        code[size++] = 0xC2;
        code[size++] = 0x04;
        code[size++] = 0x00;

        DWORD dwOldFlag, dwTempFlag;
        VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &dwOldFlag);
        WriteProcessMemory(GetCurrentProcess(), addr, code, size, NULL);
        VirtualProtect(addr, size, dwOldFlag, &dwTempFlag);
    }
}

void myInvalidParameterHandler(const wchar_t* expression,
    const wchar_t* function,
    const wchar_t* file,
    unsigned int line,
    uintptr_t pReserved)
{
    (void)expression;
    (void)function;
    (void)file;
    (void)line;
    (void)pReserved;
    throw 1;
}

void myPurecallHandler(void)
{
    throw 1;
}

void CDumpUtil::Enable(bool bEnable)
{
    if (bEnable)
    {
        ::SetUnhandledExceptionFilter(UnhandledExceptionFilter);

        /*
        * 为了安全起见（万一有某种新型的异常没有被事先安装的处理器过滤，UEF会被覆盖），
        * 你可以HOOK掉Kernel32.SetUnhandledExceptionFilter,从而禁止任何可能的覆盖。
        */
        _set_abort_behavior(0, _CALL_REPORTFAULT);
        _set_invalid_parameter_handler(myInvalidParameterHandler);
        _set_purecall_handler(myPurecallHandler);

        DisableSetUnhandledExceptionFilter();

        //LOG_DEBUG(LOG_TAG, "enable minidump");
    }
    else
    {
        ::SetUnhandledExceptionFilter(nullptr);
    }
}
