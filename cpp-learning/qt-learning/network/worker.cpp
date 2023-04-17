#include "worker.h"

Worker::Worker(qint32 t, QString s1, QString s2, QString s3, QString s4, QObject *parent) : s1(s1), s2(s2), s3(s3), s4(s4), QObject(parent)
{
    myTime = t;
}

void Worker::start()
{
    // timer = new QTimer();
    // timer->start(myTime);
    // qInfo() << QString("start work in time:%1").arg(myTime);
    // connect(timer, SIGNAL(timeout()), this, SLOT(doWork()));
    doWork();
}

void Worker::doWork()
{
    qInfo() << "dowork";
    QNetworkRequest request;
    QNetworkAccessManager* naManager = new QNetworkAccessManager();
    QUrl strUrl("http://10.20.54.2:8888/apps/adddbusproxy");
    request.setUrl(strUrl);
    request.setRawHeader("Content-Type", "charset='utf-8'");
    request.setRawHeader("Content-Type", "application/json");

    QJsonObject param;
    param.insert("appId", QJsonValue::fromVariant(s1));
    param.insert("name", QJsonValue::fromVariant(s2));
    param.insert("path", QJsonValue::fromVariant(s3));
    param.insert("interface", QJsonValue::fromVariant(s4));

    QNetworkReply* reply = naManager->post(request, QJsonDocument(param).toJson(QJsonDocument::Compact));
    QByteArray responseData;
    QEventLoop eventLoop;
    QObject::connect(naManager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    eventLoop.exec();
    QTextCodec* codec = QTextCodec::codecForName("utf8");
    QString strReply = codec->toUnicode(reply->readAll());
    reply->deleteLater();
    naManager->deleteLater();

    // timer->stop();
    emit workFinished();
}