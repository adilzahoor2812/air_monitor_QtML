#pragma once

#include "SensorSample.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class MockGenerator : public QObject
{
    Q_OBJECT

public:
    explicit MockGenerator(QObject *parent = nullptr);

    void setRunning(bool running);
    bool isRunning() const;

signals:
    void sampleReceived(const SensorSample &sample);

private:
    void tick();

    QTimer m_timer;
    QElapsedTimer m_clock;
};
