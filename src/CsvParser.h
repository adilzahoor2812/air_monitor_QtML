#pragma once

#include "SensorSample.h"

#include <QByteArray>
#include <QString>

enum class ParseStatus
{
    Ok,
    Empty,
    Header,
    Invalid
};

struct ParseResult
{
    ParseStatus status = ParseStatus::Invalid;
    SensorSample sample;
    QString error;
};

ParseResult parseSensorLine(const QByteArray &line);
