#pragma once
#include <string>

class  CImCharset
{
public:
	CImCharset();
	~CImCharset();

public:
 	/**
    @name ANSI转为Unicode
    @param src ANSI格式字符串
    @return wstring Unicode格式字符串
    */
 	static std::wstring AnsiToUnicode(const char* src);

    /**
    @name ANSI转为UTF8
    @param src ANSI格式字符串
    @return string UTF8格式字符串
    */
    static std::string AnsiToUTF8(const char* src);

    /**
    @name UTF8转为UNICODE
    @param src UTF8格式字符串
    @param srcLength, src的长度
    @return wstring Unicode格式字符串
    */
    static std::wstring UTF8ToUnicode(const char* src);

    /**
    @name UNICODE转为UTF8
    @param src Unicode格式字符串
    @return string UTF8格式字符串
    */
    static std::string UnicodeToUTF8(const wchar_t* src);

    /**
    @name UNICODE转为Gbk
    @param src Unicode格式字符串
    @return string gbk格式字符串
    */
    static std::string UnicodeToGbk(const wchar_t* src);

    /**
    @name 字符串是否为UTF8编码
    @param pBuffer 字符串
    @param size 字符串字符个数
    @return bool true表示是UTF8编码字符串，false表示非UTF8编码字符串
    */
	static bool IsUTF8(const char* pBuffer, long size);
};