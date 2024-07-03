#include <Windows.h>
#include "ImCharset.h"

using namespace std;

CImCharset::CImCharset()
{
}

CImCharset::~CImCharset()
{
}

string CImCharset::UnicodeToUTF8(const wchar_t* src)
{
	if (src == NULL || src[0] == '\0')
	{
		return "";
	}

	wstring strSrc(src);
	int length = WideCharToMultiByte(CP_UTF8, 0, strSrc.c_str(),
		-1, NULL, 0, NULL, FALSE);
	char *buf = new char[length + 1];
	WideCharToMultiByte(CP_UTF8, 0, strSrc.c_str(), -1, buf, length, NULL, FALSE);
	buf[length] = '\0';
	string dest = buf;
	delete[] buf;
	return dest;
}

string CImCharset::UnicodeToGbk(const wchar_t* szSource)
{
    if (szSource == nullptr || szSource[0] == '\0')
    {
        return "";
    }

    const UINT CP_GBK = 20936;
    int length = WideCharToMultiByte(CP_GBK, 0, szSource, -1, NULL, 0, NULL, FALSE);
    if (length <= 0)
    {
        return "";
    }

    char *szBufffer = new char[length + 1];
    WideCharToMultiByte(CP_GBK, 0, szSource, -1, szBufffer, length, NULL, FALSE);
    szBufffer[length] = '\0';
    string strResult = szBufffer;
    delete[] szBufffer;
    return strResult;
}

bool CImCharset::IsUTF8(const char* pBuffer, long size)
{
	bool IsUTF8 = true;
	unsigned char* start = (unsigned char*)pBuffer;
	unsigned char* end = (unsigned char*)pBuffer + size;
	while (start < end)
	{
		if (*start < 0x80) // (10000000): 值小于0x80的为ASCII字符     
		{
			start++;
		}
		else if (*start < (0xC0)) // (11000000): 值介于0x80与0xC0之间的为无效UTF-8字符 
		{
			IsUTF8 = false;
			break;
		}
		else if (*start < (0xE0)) // (11100000): 此范围内为2字节UTF-8字符    
		{
			if (start >= end - 1)
			{
				break;
			}

			if ((start[1] & (0xC0)) != 0x80)
			{
				IsUTF8 = false;
				break;
			}

			start += 2;
		}
		else if (*start < (0xF0)) // (11110000): 此范围内为3字节UTF-8字符   
		{
			if (start >= end - 2)
			{
				break;
			}

			if ((start[1] & (0xC0)) != 0x80 || (start[2] & (0xC0)) != 0x80)
			{
				IsUTF8 = false;
				break;
			}

			start += 3;
		}
		else
		{
			IsUTF8 = false;
			break;
		}
	}

	return IsUTF8;
}


wstring CImCharset::UTF8ToUnicode(const char* src)
{
	if (src == NULL)
	{
		return L"";
	}
	string strSrc(src);
	int length = MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), -1, NULL, 0);
	wchar_t *buf = new wchar_t[length + 1];
	MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), -1, buf, length);
	buf[length] = L'\0';
	wstring dest = buf;
	delete[] buf;
	return dest;
}

wstring CImCharset::AnsiToUnicode(const char* src)
{
	if (src == NULL || src[0] == '\0')
	{
		return L"";
	}
	string strSrc(src);
	int length = MultiByteToWideChar(CP_ACP, 0, strSrc.c_str(), -1, NULL, 0);
	wchar_t *buf = new wchar_t[length + 1];
	MultiByteToWideChar(CP_ACP, 0, strSrc.c_str(), -1, buf, length);
	buf[length] = L'\0';
	wstring dest = buf;
	delete[] buf;
	return dest;
}

string CImCharset::AnsiToUTF8(const char* src)
{
	wstring unicode = AnsiToUnicode(src);
	return UnicodeToUTF8(unicode.c_str());
}
