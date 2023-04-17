#pragma once

#include "process.h"

class ProcessManager : public QObject
{
    Q_OBJECT
public:
    ProcessManager(QObject *parent = nullptr);
    ~ProcessManager() override;


public slots:
    int removeJob();

public:
    QMap<QString, Process *> jobs;
};
