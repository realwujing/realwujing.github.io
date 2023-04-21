#include "wget.h"

Wget::Wget(QObject *parent) : QObject(parent), wgetProcess(new QProcess(this)), wgetFilePath("/usr/bin/ostree")
{
    // wgetProcess = new QProcess(this);
    // connect(wgetProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
    // connect(wgetProcess, SIGNAL(readyReadStandardError()), this, SLOT(readError()));
    // connect(wgetProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(finished(int, QProcess::ExitStatus)));

    connect(this, SIGNAL(updateProgress(QString, QString, QString)), this, SLOT(showProgress(QString, QString, QString)));
    connect(this, SIGNAL(haveDone()), this, SLOT(showDone()));
    connect(this, SIGNAL(haveError(int, QString)), this, SLOT(showError(int, QString)));
}

Wget::~Wget()
{
    delete wgetProcess;
}

bool Wget::download()
{
    bool ret = false;
    args << "--repo=/deepin/linglong/repo/repo-test" << "pull" << "--mirror" << url;
    qDebug() << "24 args" << args;
    connect(wgetProcess, &QProcess::readyReadStandardOutput, [&]()
            {
                qDebug() << "27 args" << args;
        //这里需要死循环处理，不然当wget下载完成，输出还没有读完，导致无法通知进度
        while (wgetProcess->bytesAvailable() > 0) {
            qDebug() << "30 args" << args;
            QString text = wgetProcess->readLine();
            QRegExp reg("^[Receiving objects: ]+([0-9]{1,3}%)\\s\\([0-9]{2}\\/[0-9]{2}\\)\\s(^\\d+(\\.\\d+)?\\sMB/s)\\s(^\\d+(\\.\\d+)?\\sMB)");
            reg.setCaseSensitivity(Qt::CaseInsensitive);
            qDebug() << "34 text" << text;
            if (reg.exactMatch(text)) {
                percent = reg.cap(1).toLocal8Bit();
                speed = reg.cap(2).toLocal8Bit();
                fileSize = reg.cap(3).toLocal8Bit();
                qDebug() << "percent:" << percent << "speed:" << speed << "fileSize:" << fileSize;
                emit updateProgress(percent, speed, fileSize);
            }
        } });
    connect(wgetProcess, &QProcess::readyReadStandardError, [&]()
            {
        qWarning() << wgetProcess->readAllStandardError();
        emit haveError(2, wgetProcess->readAllStandardError()); });
    wgetProcess->setReadChannel(QProcess::StandardOutput);
    wgetProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    qDebug() << wgetFilePath << args;
    wgetProcess->start(wgetFilePath, args);
    ret = wgetProcess->waitForFinished(-1);
    if (!ret)
    {
        qDebug() << wgetProcess->errorString();
        emit haveError(2, wgetProcess->errorString());
    }
    // wgetProcess->setProgram(wgetFilePath);
    // wgetProcess->setArguments(args);
    // wgetProcess->startDetached();
    return ret;
}

void Wget::showProgress(QString percent, QString speed, QString fileSize)
{
    qDebug() << "percent:" << percent << "speed:" << speed << "fileSize:" << fileSize;
}

void Wget::showDone()
{
    qDebug() << "download done";
}

void Wget::showError(int code, QString msg)
{
    qDebug() << "error:" << code << msg;
}
