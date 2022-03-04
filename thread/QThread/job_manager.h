#pragma once

#include "job.h"

class JobManager : public QObject
{
    Q_OBJECT
public:
    JobManager(QObject *parent = nullptr);
    ~JobManager() override;


public slots:
    int removeJob();

public:
    QMap<QString, Job *> jobs;
};
