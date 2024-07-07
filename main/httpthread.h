#ifndef HTTPTHREAD_H
#define HTTPTHREAD_H

#include <QThread>
#include <QMap>
#include <curl/curl.h>

class ProxyServer
{
public:
    // 服务器IP
    QString m_ip;

    // 服务器端口
    int m_port = 0;
};

class HttpThread : public QThread
{
    Q_OBJECT

public:
    explicit HttpThread(QObject *parent = nullptr);

protected:
    virtual int getTimeOutSeconds() { return 3; }

    virtual QVector<QString> getCommonHeaders() { return QVector<QString>(); }

    CURL* makeRequest(QString url,
                      const QMap<QString,QString>& headers,
                      const QMap<QString,QString>& cookies,
                      ProxyServer proxyServer);

    void setPostMethod(CURL* curl, const QString& body);

    void getResponse(CURL* curl, long& statusCode, QString& data);

    // 获取服务端的Set-Cookies
    QMap<QString, QString> getCookies(CURL* curl);

    void freeRequest(CURL* curl);

private:
    QMap<CURL*, std::string*> m_bodies;

    QMap<CURL*, struct curl_slist*> m_requestHeaders;

    QMap<CURL*, std::string*> m_responseHeaders;
};

#endif // HTTPTHREAD_H
