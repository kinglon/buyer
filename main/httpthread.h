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
    virtual void onRun() = 0;

    virtual int getTimeOutSeconds() { return 3; }

    virtual QVector<QString> getHeaders() { return QVector<QString>(); }

    CURL* makeRequest(QString url, const QMap<QString,QString>& cookies, ProxyServer proxyServer);

    void setPostMethod(CURL* curl, const QString& body);

    void getResponse(CURL* curl, long& statusCode, QString& data);

    void freeRequest(CURL* curl);

private:
    void run() override;

protected:
    CURLM* m_multiHandle = nullptr;

private:
    QMap<CURL*, void*> m_writeDataBuffers;
};

#endif // HTTPTHREAD_H
