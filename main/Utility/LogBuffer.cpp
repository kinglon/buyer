#include <Windows.h>
#include "LogBuffer.h"
#include <time.h>
#include <conio.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include "ImCharset.h"

using namespace std;

#ifdef WIN32
const char PATH_SEPERATOR = L'\\';
#else
const char PATH_SEPERATOR = L'/';
#endif

#define LINE_BREAK_SIZE 3

LogBuffer::LogBuffer(int bufferSize)
{
	m_szBuff = new wchar_t[bufferSize];
	memset(m_szBuff, 0, bufferSize * sizeof(wchar_t));
	m_pBuffWritten = m_szBuff;
	m_pBuffEnd = m_pBuffWritten + bufferSize - 1; // 1 for \0
}

LogBuffer::~LogBuffer()
{
	delete[] m_szBuff;
}

void LogBuffer::LogFileName(const char* pFilePathName, unsigned int nLine)
{
	if (pFilePathName != nullptr){
		const char* pFileName = strrchr(const_cast<char*>(pFilePathName), PATH_SEPERATOR);
		if (pFileName) {
			pFileName++;
		}
		else{
			pFileName = pFilePathName;
		}

        int nWtiteDone = _snwprintf_s(m_pBuffWritten, m_pBuffEnd - m_pBuffWritten, _TRUNCATE, L"[%S:%d] ", pFileName, nLine);
		if (nWtiteDone > 0){
			m_pBuffWritten += nWtiteDone;
		}
	}
}

void LogBuffer::LogTimeInfoAndLevel(ELogLevel logLevel)
{
    wstring strLevel = L"Info";
    if (logLevel >= ELogLevel::LOG_LEVEL_ERROR)
    {
        strLevel = L"Error";
    }
	else if (logLevel < ELogLevel::LOG_LEVEL_INFO)
	{
		strLevel = L"Debug";
	}

	SYSTEMTIME st;
	GetLocalTime(&st);
    int nWtiteDone = _snwprintf_s(m_pBuffWritten, m_pBuffEnd - m_pBuffWritten, _TRUNCATE, L"<<%02d-%02d-%02d %02d:%02d:%02d#%s>>", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, strLevel.c_str());
	if (nWtiteDone > 0)
    {
		m_pBuffWritten += nWtiteDone;
	}
}

void LogBuffer::LogTID()
{
	DWORD dwTID = GetCurrentThreadId();
    int nWtiteDone = _snwprintf_s(m_pBuffWritten, m_pBuffEnd - m_pBuffWritten, _TRUNCATE, L"[tid:%05d] ", dwTID);
	if (nWtiteDone > 0)
	{
		m_pBuffWritten += nWtiteDone;
	}
}

void LogBuffer::LogString(const wchar_t* pString)
{
	if (pString == nullptr)
	{
		return;
	}

	int bufCount = (m_pBuffEnd - m_pBuffWritten) - LINE_BREAK_SIZE;
    int nWtiteDone = _snwprintf_s(m_pBuffWritten, bufCount, _TRUNCATE, L"[%s] ", pString);
	if (nWtiteDone > 0)
	{
		m_pBuffWritten += nWtiteDone;
	}
	else if (nWtiteDone == -1)  //填充满
	{
		m_pBuffWritten += bufCount -1; // 去掉\0
	}
}

void LogBuffer::LogFormat(const wchar_t* fmt, va_list ap)
{
	int bufCount = (m_pBuffEnd - m_pBuffWritten) - LINE_BREAK_SIZE;
    int nWtiteDone = _vsnwprintf_s(m_pBuffWritten, bufCount, _TRUNCATE, fmt, ap);
	if (nWtiteDone > 0)
	{
		m_pBuffWritten += nWtiteDone;
	}
    else if (nWtiteDone == -1)  //填充满
    {
        m_pBuffWritten += bufCount - 1; // 去掉\0
    }    
}

void LogBuffer::AppendLineBreak()
{
	if (m_pBuffEnd - m_pBuffWritten >= LINE_BREAK_SIZE)
    {
		m_pBuffWritten[0] = L'\r';
		m_pBuffWritten[1] = L'\n';
		m_pBuffWritten[2] = L'\0';
		m_pBuffWritten += LINE_BREAK_SIZE;
	}
}

LogBuffer::operator const wchar_t*()
{
	return m_szBuff;
}

unsigned long LogBuffer::GetLogLength()
{
	return m_pBuffWritten - m_szBuff;
}
