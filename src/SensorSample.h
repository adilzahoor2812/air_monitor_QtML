#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct SensorSample
{
    qint64 timestampMs = 0;
    double distanceCm = 0.0;
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    double gx = 0.0;
    double gy = 0.0;
    double gz = 0.0;
    QString state;
    bool valid = false;
};

Q_DECLARE_METATYPE(SensorSample)
