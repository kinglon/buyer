#include "httpthread.h"

size_t writeCallback(void *contents, size_t sz, size_t nmemb, std::string *body)
{
    size_t realsize = sz * nmemb;
    body->append((char*)contents, realsize);
    return realsize;
}

size_t headerCallback(char* data, size_t size, size_t nmemb, std::string* headers)
{
    size_t dataSize = size * nmemb;
    headers->append(data, dataSize);
    return dataSize;
}

HttpThread::HttpThread(QObject *parent)
    : QThread{parent}
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CURL* HttpThread::makeRequest(QString url,
                              const QMap<QString,QString>& headers,
                              const QMap<QString,QString>& cookies,
                              ProxyServer proxyServer)
{
    CURL *handle = curl_easy_init();
    if (handle == nullptr)
    {
        return nullptr;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url.toStdString().c_str());

    std::string* body = new std::string();
    body->reserve(500*1024);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, body);
    m_bodies[handle] = body;

    std::string* headerBuffer = new std::string();
    headerBuffer->reserve(50*1024);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, headerBuffer);
    m_responseHeaders[handle] = headerBuffer;

    if (!proxyServer.m_ip.isEmpty())
    {
        QString proxyServerHead = QString("socks5://%1:%2").arg(proxyServer.m_ip, QString::number(proxyServer.m_port));
        curl_easy_setopt(handle, CURLOPT_PROXY, proxyServerHead.toStdString().c_str());
    }
    else
    {
        curl_easy_setopt(handle, CURLOPT_PROXY, "");
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

    curl_easy_setopt(handle, CURLOPT_TIMEOUT, getTimeOutSeconds());
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, getTimeOutSeconds()*1000);
    curl_easy_setopt(handle, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "gzip, deflate, br, zstd");
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36");

    QMap<QString,QString> headerSummary;
    headerSummary["Content-Type"] = "application/json";
    headerSummary["Accept"] = "application/json, text/plain, */*";
    headerSummary["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";

    QMap<QString,QString> commonHeaders = getCommonHeaders();
    for (auto it=commonHeaders.begin(); it != commonHeaders.end(); it++)
    {
        headerSummary[it.key()] = it.value();
    }
    for (auto it=headers.begin(); it!=headers.end(); it++)
    {
        headerSummary[it.key()] = it.value();
    }

    struct curl_slist* curlHeaders = nullptr;
    for (auto it = headerSummary.begin(); it != headerSummary.end(); it++)
    {
        QString h = it.key() + ": " + it.value();
        curlHeaders = curl_slist_append(curlHeaders, h.toStdString().c_str());
    }
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, curlHeaders);
    m_requestHeaders[handle] = curlHeaders;

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

    auto it = m_bodies.find(curl);
    if (it != m_bodies.end())
    {        
        data = QString::fromUtf8((*it)->c_str(), (*it)->length());
    }
}

QMap<QString, QString> HttpThread::getCookies(CURL* curl)
{
    QMap<QString, QString> cookies;
    auto it = m_responseHeaders.find(curl);
    if (it == m_responseHeaders.end())
    {
        return cookies;
    }

    std::string& headers = *it.value();
    size_t pos = headers.find("Set-Cookie:");
    while (pos != std::string::npos)
    {
        pos += sizeof("Set-Cookie:");
        size_t end = headers.find("\r\n", pos);
        if (end != std::string::npos)
        {
            std::string setCookieHeader = headers.substr(pos, end - pos);

            // Parse the key and value of the cookie
            size_t equalsPos = setCookieHeader.find('=');
            if (equalsPos != std::string::npos)
            {
                std::string key = setCookieHeader.substr(0, equalsPos);
                int fenhaoPos = setCookieHeader.find(';');
                std::string value = setCookieHeader.substr(equalsPos + 1);
                if (fenhaoPos != -1)
                {
                    value = setCookieHeader.substr(equalsPos + 1, fenhaoPos - equalsPos -1);
                }

                // Remove leading and trailing spaces from the key and value
                size_t keyStart = key.find_first_not_of(" \t");
                size_t keyEnd = key.find_last_not_of(" \t");
                size_t valueStart = value.find_first_not_of(" \t");
                size_t valueEnd = value.find_last_not_of(" \t");

                if (keyStart != std::string::npos && keyEnd != std::string::npos && valueStart != std::string::npos && valueEnd != std::string::npos)
                {
                    key = key.substr(keyStart, keyEnd - keyStart + 1);
                    value = value.substr(valueStart, valueEnd - valueStart + 1);
                    cookies[key.c_str()] = value.c_str();
                }
            }
        }

        // Find the next occurrence of "Set-Cookie:"
        pos = headers.find("Set-Cookie:", pos);
    }

    return cookies;
}

void HttpThread::freeRequest(CURL* curl)
{    
    auto itBody = m_bodies.find(curl);
    if (itBody != m_bodies.end())
    {
        delete (*itBody);
        m_bodies.erase(itBody);
    }

    auto itRequestHeader = m_requestHeaders.find(curl);
    if (itRequestHeader != m_requestHeaders.end())
    {
        curl_slist_free_all(*itRequestHeader);
        m_requestHeaders.erase(itRequestHeader);
    }

    auto itResponseHeader = m_responseHeaders.find(curl);
    if (itResponseHeader != m_responseHeaders.end())
    {
        delete (*itResponseHeader);
        m_responseHeaders.erase(itResponseHeader);
    }

    curl_easy_cleanup(curl);
}
