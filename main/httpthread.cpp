#include "httpthread.h"

typedef struct {
  char *buf;
  size_t size;
} memory;

size_t grow_buffer(void *contents, size_t sz, size_t nmemb, void *ctx)
{
  size_t realsize = sz * nmemb;
  memory *mem = (memory*) ctx;
  char *ptr = (char*)realloc(mem->buf, mem->size + realsize);
  if(!ptr) {
      qCritical("not enough memory");
      return 0;
  }
  mem->buf = ptr;
  memcpy(&(mem->buf[mem->size]), contents, realsize);
  mem->size += realsize;
  return realsize;
}

HttpThread::HttpThread(QObject *parent)
    : QThread{parent}
{

}

void HttpThread::run()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_multiHandle = curl_multi_init();
    if (m_multiHandle == nullptr)
    {
        qCritical("failed to create multi handle");
        return;
    }

    onRun();

    curl_multi_cleanup(m_multiHandle);
}


CURL* HttpThread::makeRequest(QString url, const QMap<QString,QString>& cookies, ProxyServer proxyServer)
{
  CURL *handle = curl_easy_init();
  if (handle == nullptr)
  {
      return nullptr;
  }

  curl_easy_setopt(handle, CURLOPT_URL, url.toStdString().c_str());

  /* buffer body */
  memory *mem = (memory*)malloc(sizeof(memory));
  mem->size = 0;
  mem->buf = (char*)malloc(1);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, grow_buffer);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, mem);
  m_writeDataBuffers[handle] = mem;

  curl_easy_setopt(handle, CURLOPT_TIMEOUT, getTimeOutSeconds());
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, getTimeOutSeconds());
  curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);

  curl_easy_setopt(handle, CURLOPT_HEADER, "Content-Type: application/json");
  curl_easy_setopt(handle, CURLOPT_HEADER, "Accept: application/json, text/plain, */*");
  curl_easy_setopt(handle, CURLOPT_HEADER, "Accept-Encoding: gzip, deflate, br, zstd");
  curl_easy_setopt(handle, CURLOPT_HEADER, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
  curl_easy_setopt(handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");

  if (!proxyServer.m_ip.isEmpty())
  {
      QString proxyServerHead = QString("socks5://%1:%2").arg(proxyServer.m_ip, proxyServer.m_port);
      curl_easy_setopt(handle, CURLOPT_PROXY, proxyServerHead.toStdString().c_str());
  }

  if (!cookies.empty())
  {
      QString cookieHead;
      for (auto it = cookies.begin(); it != cookies.end(); it++)
      {
          if (!cookieHead.isEmpty())
          {
              cookieHead += "; ";
          }
          cookieHead += it.key() + "=" + it.value();
      }
      curl_easy_setopt(handle, CURLOPT_COOKIE, cookieHead.toStdString().c_str());
  }

  for (const auto& header : getHeaders())
  {
      curl_easy_setopt(handle, CURLOPT_HEADER, header.toStdString().c_str());
  }

  return handle;
}

void HttpThread::setPostMethod(CURL* curl, const QString& body)
{
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body.toStdString().c_str());
}

void HttpThread::getResponse(CURL* curl, long& statusCode, QString& data)
{
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    auto it = m_writeDataBuffers.find(curl);
    if (it != m_writeDataBuffers.end())
    {
        memory* mem = (memory*)it.value();
        data = QString::fromUtf8(mem->buf, mem->size);
    }
}

void HttpThread::freeRequest(CURL* curl)
{
    memory *mem = nullptr;
    auto it = m_writeDataBuffers.find(curl);
    if (it != m_writeDataBuffers.end())
    {
        mem = (memory*)it.value();
        m_writeDataBuffers.erase(it);
    }
    free(mem->buf);
    free(mem);

    curl_easy_cleanup(curl);
}
