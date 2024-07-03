#pragma once

#include <wchar.h>
#include "LogUtil.h"

//消息缓冲区大小
#define LOG_BUFFER_SIZE 1024*5

class LogBuffer
{
public:
	LogBuffer(int bufferSize);
	~LogBuffer();

public:
	void LogFileName(const char* pFilePathName, unsigned int nLine);
	void LogTimeInfoAndLevel(ELogLevel logLevel);
	void LogTID();
	void LogString(const wchar_t* pString);
	void LogFormat(const wchar_t* fmt, va_list ap);
	void AppendLineBreak();
	operator const wchar_t*();
	unsigned long GetLogLength();

protected:
    wchar_t* m_szBuff = nullptr;
    wchar_t*	m_pBuffWritten = nullptr;
    const wchar_t*	m_pBuffEnd = nullptr;  // 最后一个字符，不能写
};