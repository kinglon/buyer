#include <Windows.h>
#include "LogUtil.h"
#include "LogBuffer.h"
#include "ImCharset.h"
#include "ImPath.h"
#include "LogMacro.h"

using namespace std;

map<std::wstring, CLogUtil*> CLogUtil::s_mapLogName2LogUtil;
CCSWrap CLogUtil::s_csLogMapObject;

CLogUtil::CLogUtil(const wchar_t* szLogFileName)
{
    OpenLog(szLogFileName);
}

CLogUtil::~CLogUtil()
{
    CloseLog();
}

CLogUtil* CLogUtil::GetLog(const wchar_t* szLogFileName)
{
    if (szLogFileName == nullptr || wcslen(szLogFileName) == 0)
    {
        return nullptr;
    }

    CIcrCriticalSection cs(s_csLogMapObject.GetCS());
    auto it = s_mapLogName2LogUtil.find(szLogFileName);
    if (it != s_mapLogName2LogUtil.end())
    {
        return it->second;
    }
    else
    {
        CLogUtil* pLogUtil = new CLogUtil(szLogFileName);
        s_mapLogName2LogUtil[szLogFileName] = pLogUtil;
        return pLogUtil;
    }
}

void CLogUtil::SetLogLevel(ELogLevel nLogLevel)
{
    CIcrCriticalSection cs(m_csLog.GetCS());
    m_logLevel = nLogLevel;
}

void CLogUtil::SetLogBufferSize(int bufferSize) 
{ 
    LOG_INFO(L"set the buffer size of log to %d", bufferSize);

    CIcrCriticalSection cs(m_csLog.GetCS());
    m_logBufferSize = bufferSize; 
}

void CLogUtil::Log(const char* pFileName, unsigned int nLine, ELogLevel nLogLevel, const wchar_t* szFormat, ...)
{    
    if (m_fpFile == nullptr)
    {        
        return;
    }

    if (nLogLevel < m_logLevel)
    {
        return;
    }

    LogBuffer logBuff(m_logBufferSize);
    logBuff.LogTimeInfoAndLevel(nLogLevel);
    logBuff.LogTID();
    if (pFileName != nullptr)
    {
        logBuff.LogFileName(pFileName, nLine);
    }

    va_list valist;
    va_start(valist, szFormat);    
    logBuff.LogFormat(szFormat, valist);
    va_end(valist);

    logBuff.AppendLineBreak();

    const wchar_t* pLogBuff = logBuff;
    CIcrCriticalSection csStartLog(m_csLog.GetCS());
    fputws(pLogBuff, m_fpFile);
    fflush(m_fpFile);
    csStartLog.Leave();
}

void CLogUtil::OpenLog(const wchar_t* szLogFileName)
{
    CloseLog();    

    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t szLogFile[MAX_PATH];
    memset(szLogFile, 0, sizeof(szLogFile));
    _snwprintf_s(szLogFile, MAX_PATH, MAX_PATH, L"%s%s.%4d%02d%02d.log", CImPath::GetLogPath().c_str(), szLogFileName, st.wYear, st.wMonth, st.wDay); //如C:\\BERR.20111213.log
    m_fpFile = _wfsopen(szLogFile, L"a+b", _SH_DENYNO);
    if (m_fpFile)
    {
        fwprintf(m_fpFile, L"\n\n\nLog begin at:%d:%d:%d\n", st.wHour, st.wMinute, st.wSecond);        
        fflush(m_fpFile);
    }

    //todo by yjl 每天一个日志，30天清除日志
}

void CLogUtil::CloseLog()
{
    CIcrCriticalSection cs(m_csLog.GetCS());
    if (m_fpFile)
    {
        fclose(m_fpFile);
        m_fpFile = nullptr;
    }
}
