#pragma once

#include "download.h"

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    DownloadManager(QObject *parent = nullptr);
    ~DownloadManager() override;


public slots:
    int removeJob();

public:
    QMap<QString, Download *> jobs;
};
