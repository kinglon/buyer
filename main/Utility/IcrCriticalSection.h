#pragma once

#include <Windows.h>

/***
临界区对象，构造函数自动进入临界区，析构函数自动释放临界区，析构之前可手动调Leave提前离开临界区
*/
class CIcrCriticalSection
{
public:
    CIcrCriticalSection(CRITICAL_SECTION* pCS);
    ~CIcrCriticalSection();

public:
    /**
    @name 离开临界区
    */
    void Leave();

private:
    CRITICAL_SECTION* m_pCS = nullptr;
    bool m_bAlreadyLeave = false;
};

/**
将临界区变量封装成对象，利用构造函数初始化，析构函数释放资源
*/
class CCSWrap
{
public:
    CCSWrap()  { InitializeCriticalSection(&m_cs); }
    ~CCSWrap() { DeleteCriticalSection(&m_cs); }

public:
    CRITICAL_SECTION* GetCS() { return &m_cs; }

private:
    CRITICAL_SECTION m_cs;
};
