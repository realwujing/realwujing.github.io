#pragma once

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTextCodec>
#include <QJsonDocument>
#include <QJsonObject>

class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(qint32, QString s1, QString s2, QString s3, QString s4, QObject *parent = 0);
    qint32 myTime;

signals:
    void workFinished();

public slots:
    void doWork();
    void start();

public:
    QString s1;
    QString s2; 
    QString s3; 
    QString s4;

private:
    QTimer *timer;
};