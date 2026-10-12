#include "CsvRecorder.h"

#include <QDateTime>
#include <QDir>

namespace
{
QByteArray formatSample(const SensorSample &sample)
{
    const auto number = [](double value) {
        return QString::number(value, 'f', 6);
    };
    const QString line = QString::number(sample.timestampMs) + QLatin1Char(',')
        + number(sample.distanceCm) + QLatin1Char(',')
        + number(sample.ax) + QLatin1Char(',')
        + number(sample.ay) + QLatin1Char(',')
        + number(sample.az) + QLatin1Char(',')
        + number(sample.gx) + QLatin1Char(',')
        + number(sample.gy) + QLatin1Char(',')
        + number(sample.gz) + QLatin1Char(',')
        + sample.state + QLatin1Char('\n');
    return line.toUtf8();
}
}

CsvRecorder::CsvRecorder(QObject *parent)
    : QObject(parent)
{
}

bool CsvRecorder::start(const QString &directory)
{
    if (m_file.isOpen())
        stop();

    m_error.clear();
    m_rows = 0;
    m_path.clear();

    const QString trimmed = directory.trimmed();
    if (trimmed.isEmpty()) {
        m_error = QStringLiteral("Recording directory is empty");
        return false;
    }

    QDir dir(trimmed);
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        m_error = QStringLiteral("Could not create recording directory: %1").arg(dir.absolutePath());
        return false;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_path = dir.filePath(QStringLiteral("sensor_%1.csv").arg(stamp));
    m_file.setFileName(m_path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_error = QStringLiteral("Could not open %1 for writing").arg(m_path);
        m_path.clear();
        return false;
    }

    const QByteArray header = QByteArrayLiteral("timestamp_ms,distance_cm,ax,ay,az,gx,gy,gz,state\n");
    if (m_file.write(header) != header.size()) {
        m_error = QStringLiteral("Could not write the CSV header to %1").arg(m_path);
        m_file.close();
        m_file.remove();
        m_path.clear();
        return false;
    }
    m_file.flush();
    return true;
}

void CsvRecorder::stop()
{
    if (!m_file.isOpen())
        return;
    m_file.flush();
    m_file.close();
}

bool CsvRecorder::write(const SensorSample &sample)
{
    if (!m_file.isOpen()) {
        m_error = QStringLiteral("Recording is not active");
        return false;
    }
    if (!sample.valid || sample.state.isEmpty()) {
        m_error = QStringLiteral("Refusing to record an invalid sample");
        return false;
    }

    const QByteArray line = formatSample(sample);
    if (m_file.write(line) != line.size()) {
        m_error = QStringLiteral("Write failed for %1").arg(m_path);
        stop();
        return false;
    }
    if (!m_file.flush()) {
        m_error = QStringLiteral("Flush failed for %1").arg(m_path);
        stop();
        return false;
    }
    ++m_rows;
    return true;
}

bool CsvRecorder::isRecording() const
{
    return m_file.isOpen();
}

QString CsvRecorder::path() const
{
    return m_path;
}

int CsvRecorder::rowCount() const
{
    return m_rows;
}

QString CsvRecorder::lastError() const
{
    return m_error;
}
