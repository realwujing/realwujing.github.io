#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QDebug>
#include <QEventLoop>

class Wget : public QObject
{
    Q_OBJECT

public:
    Wget(QObject *parent = nullptr);
    ~Wget();

    bool download();

public slots:
    void showProgress(QString percent, QString speed, QString fileSize);
    void showDone();
    void showError(int code, QString msg);

signals:
    void updateProgress(QString percent, QString speed, QString fileSize);
    void haveDone();
    void haveError(int code, QString msg);

public:
    QProcess *wgetProcess;
    QString wgetFilePath = "wget";
    QString url;
    QStringList args;
    QString percent;
    QString speed;
    QString fileSize;
};