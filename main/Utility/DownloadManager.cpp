#include "DownloadManager.h"
#include <wininet.h>
#include <thread>
#include "LogMacro.h"

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Wininet.lib")

int CDownloadManager::m_currentTaskId = 0;
bool CDownloadManager::m_cancelCurrentTask = false;

CDownloadManager* CDownloadManager::GetInstance()
{
	static CDownloadManager* instance = new CDownloadManager();
	return instance;
}

int CDownloadManager::CreateDownloadTask(const std::wstring& url, const std::wstring& savedFilePath, IDownloadCallback* callback)
{
	if (url.empty() || savedFilePath.empty() || callback == nullptr)
	{
		LOG_ERROR(L"failed to create a downloading task, error: invalid param");
		return 0;
	}

	// 添加一个任务项
	static int g_taskId = 0;
	g_taskId++;
	CTaskItem taskItem;
	taskItem.m_taskId = g_taskId;
	taskItem.m_url = url;
	taskItem.m_savedFilePath = savedFilePath;
	taskItem.m_callback = callback;

	CIcrCriticalSection lock(m_cs.GetCS());
	m_tasks.push_back(taskItem);
	lock.Leave();

	// 如果线程还没启动，启动线程
	static bool launch = false;
	if (!launch)
	{
		new std::thread(&CDownloadManager::ThreadProc, this);
		launch = true;
	}

	return g_taskId;
}

void CDownloadManager::ThreadProc()
{
	LOG_INFO(L"the thread of downloading file is running");

	while (true)
	{		
		CIcrCriticalSection lock(m_cs.GetCS());
		if (m_tasks.empty())
		{
			lock.Leave();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		CTaskItem taskItem = m_tasks[0];
		m_tasks.erase(m_tasks.begin());
		lock.Leave();

		DeleteUrlCacheEntry(taskItem.m_url.c_str());
		m_currentTaskId = taskItem.m_taskId;
		m_cancelCurrentTask = false;

		CBindStatusCallback bindStatusCallback;
		bindStatusCallback.m_callback = taskItem.m_callback;
		bindStatusCallback.m_taskId = taskItem.m_taskId;
		LRESULT lr = URLDownloadToFile(nullptr, taskItem.m_url.c_str(), taskItem.m_savedFilePath.c_str(), 
			0, &bindStatusCallback);
		if (lr != S_OK)
		{
			LOG_ERROR(L"failed to call URLDownloadToFile, error is 0x%x", lr);
		}

		if (!m_cancelCurrentTask)
		{
			taskItem.m_callback->OnDownloadFinish(taskItem.m_taskId, lr == S_OK);
		}
	}	
}

void CDownloadManager::CancelTask(int taskId)
{
	if (taskId == m_currentTaskId)
	{
		m_cancelCurrentTask = true;
		return;
	}

	CIcrCriticalSection lock(m_cs.GetCS());
	for (auto it = m_tasks.begin(); it != m_tasks.end(); it++)
	{
		if (it->m_taskId == taskId)
		{			
			m_tasks.erase(it);
			break;
		}
	}
}
