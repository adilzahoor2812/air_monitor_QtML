#include "CsvParser.h"

#include <QList>
#include <QtMath>

namespace
{
QByteArray trimmedField(const QByteArray &field)
{
    return field.trimmed();
}

bool readInteger(const QByteArray &text, const char *name, qint64 *value, QString *error)
{
    bool ok = false;
    const qint64 parsed = text.toLongLong(&ok);
    if (!ok) {
        *error = QStringLiteral("%1 is not an integer").arg(QLatin1String(name));
        return false;
    }
    *value = parsed;
    return true;
}

bool readFinite(const QByteArray &text, const char *name, double *value, QString *error)
{
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (!ok || !qIsFinite(parsed)) {
        *error = QStringLiteral("%1 is not a finite number").arg(QLatin1String(name));
        return false;
    }
    *value = parsed;
    return true;
}
}

ParseResult parseSensorLine(const QByteArray &line)
{
    ParseResult result;
    QByteArray text = line;
    if (text.startsWith("\xEF\xBB\xBF"))
        text.remove(0, 3);
    text = text.trimmed();

    if (text.isEmpty() || text.startsWith('#')) {
        result.status = ParseStatus::Empty;
        return result;
    }

    const QList<QByteArray> fields = text.split(',');
    if (fields.size() != 9) {
        result.status = ParseStatus::Invalid;
        result.error = QStringLiteral("Expected 9 comma-separated fields, got %1").arg(fields.size());
        return result;
    }

    const QByteArray timestampField = trimmedField(fields.at(0));
    if (timestampField.compare("timestamp_ms", Qt::CaseInsensitive) == 0) {
        result.status = ParseStatus::Header;
        return result;
    }

    SensorSample sample;
    if (!readInteger(timestampField, "timestamp_ms", &sample.timestampMs, &result.error)
        || !readFinite(trimmedField(fields.at(1)), "distance_cm", &sample.distanceCm, &result.error)
        || !readFinite(trimmedField(fields.at(2)), "ax", &sample.ax, &result.error)
        || !readFinite(trimmedField(fields.at(3)), "ay", &sample.ay, &result.error)
        || !readFinite(trimmedField(fields.at(4)), "az", &sample.az, &result.error)
        || !readFinite(trimmedField(fields.at(5)), "gx", &sample.gx, &result.error)
        || !readFinite(trimmedField(fields.at(6)), "gy", &sample.gy, &result.error)
        || !readFinite(trimmedField(fields.at(7)), "gz", &sample.gz, &result.error)) {
        result.status = ParseStatus::Invalid;
        return result;
    }

    const QString state = QString::fromLatin1(trimmedField(fields.at(8))).toUpper();
    if (state != QLatin1String("OK") && state != QLatin1String("WARNING")
        && state != QLatin1String("FAULT")) {
        result.status = ParseStatus::Invalid;
        result.error = QStringLiteral("state must be OK, WARNING, or FAULT");
        return result;
    }

    sample.state = state;
    sample.valid = true;
    result.status = ParseStatus::Ok;
    result.sample = sample;
    return result;
}
