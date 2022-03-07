#pragma once

#include "job2.h"

class JobManager2 : public QObject
{
    Q_OBJECT
public:
    JobManager2(QObject *parent = nullptr);
    ~JobManager2() override;


public slots:
    int removeJob();

public:
    QMap<QString, Job2 *> jobs;
};
