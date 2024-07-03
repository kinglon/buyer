#include "IcrCriticalSection.h"

CIcrCriticalSection::CIcrCriticalSection(CRITICAL_SECTION* pCS)
{
    m_pCS = pCS;
    if (m_pCS)
    {
        EnterCriticalSection(m_pCS);
    }    
}

CIcrCriticalSection::~CIcrCriticalSection()
{
    if (!m_bAlreadyLeave)
    {
        if (m_pCS)
        {
            LeaveCriticalSection(m_pCS);
        }        
    }    
}

void CIcrCriticalSection::Leave()
{
    if (!m_bAlreadyLeave)
    {
        m_bAlreadyLeave = true;
        if (m_pCS)
        {
            LeaveCriticalSection(m_pCS);
        }
    }
}
