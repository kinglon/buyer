#ifndef GOODSBUYER_H
#define GOODSBUYER_H

#include <QThread>
#include <QObject>
#include "httpthread.h"

class GoodsBuyer : public HttpThread
{
    Q_OBJECT

public:
    explicit GoodsBuyer(QObject *parent = nullptr);

public:
    void requestStop() { m_requestStop = true; }

protected:
    void onRun() override;

signals:
    void printLog(QString content);

    void buyFinish(GoodsBuyer* buyer);

private:
    bool m_requestStop = false;
};

#endif // GOODSBUYER_H
