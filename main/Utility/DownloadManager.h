#pragma once

#include <Urlmon.h>
#include <string>
#include <queue>
#include "IcrCriticalSection.h"

class IDownloadCallback
{
public:
    // download progress, progress is [0-100]
    virtual void OnDownloadProgress(int taskId, int progress) { (void)taskId; (void)progress; }

    // download completely callback
    virtual void OnDownloadFinish(int taskId, bool isSuccess) { (void)taskId; (void)isSuccess; }
};

class CTaskItem
{
public:
	// 任务ID
	int m_taskId = 0;

	// 下载URL
	std::wstring m_url;

	// 保存文件地址
	std::wstring m_savedFilePath;

	// 回调
	IDownloadCallback* m_callback = nullptr;
};

class CDownloadManager
{
	friend class CBindStatusCallback;

public:
	static CDownloadManager* GetInstance();

public:
	// 创建一个下载任务，返回任务ID
	// 返回0表示失败
	int CreateDownloadTask(const std::wstring& url, const std::wstring& savedFilePath, IDownloadCallback* callback);

	// 取消下载任务
	void CancelTask(int taskId);

private:
	void ThreadProc();

private:
	// 同步对象
	CCSWrap m_cs;

	// 任务列表
	std::vector<CTaskItem> m_tasks;

	// 当前下载的任务
	static int m_currentTaskId;

	// 标志是否取消当前任务
	static bool m_cancelCurrentTask;
};

class CBindStatusCallback : public IBindStatusCallback
{
public:
	HRESULT __stdcall QueryInterface(const IID &, void **) 
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef(void) 
	{
		return 1;
	}

	ULONG STDMETHODCALLTYPE Release(void) 
	{
		return 1;
	}

    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD , IBinding *)
	{
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG *) {
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD )
	{
		return S_OK;
	}

    virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT , LPCWSTR )
	{
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *, BINDINFO *)
	{
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD , DWORD , FORMATETC *, STGMEDIUM *)
	{
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID , IUnknown *)
	{
		return E_NOTIMPL;
	}

    virtual HRESULT STDMETHODCALLTYPE OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG , LPCWSTR )
	{
		if (CDownloadManager::m_cancelCurrentTask)
		{
			return E_ABORT;
		}

		if (m_callback == nullptr)
		{
			return E_ABORT;
		}

		if (ulProgressMax == 0)
		{
			return S_OK;
		}

		int progress = (int)(ulProgress * 100.0f / ulProgressMax);
		if (progress > m_currentProgress && progress <= 100)
		{
			m_currentProgress = progress;
			m_callback->OnDownloadProgress(m_taskId, progress);
		}

		return S_OK;
	}

public:
	// 当前任务ID
	int m_taskId = 0;

	// 回调对象
	IDownloadCallback* m_callback = nullptr;

	// 当前进度
	int m_currentProgress = 0;
};


