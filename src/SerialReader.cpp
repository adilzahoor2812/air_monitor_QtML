#include "SerialReader.h"

#include "CsvParser.h"

#include <QSerialPort>

SerialReader::SerialReader(QObject *parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialReader::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialReader::onError);
}

bool SerialReader::open(const QString &portName, int baudRate)
{
    if (m_port.isOpen())
        close();

    m_requestedName = portName;
    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadOnly)) {
        m_lastError = describeOpenError();
        return false;
    }

    m_port.clear(QSerialPort::AllDirections);
    m_lines.clear();
    m_lastError.clear();
    emit opened();
    return true;
}

void SerialReader::close()
{
    if (!m_port.isOpen())
        return;

    m_closing = true;
    m_port.close();
    m_closing = false;
    m_lines.clear();
    emit closed(QString());
}

bool SerialReader::isOpen() const
{
    return m_port.isOpen();
}

QString SerialReader::lastError() const
{
    return m_lastError;
}

QString SerialReader::describeOpenError() const
{
    const QString name = m_requestedName.isEmpty() ? m_port.portName() : m_requestedName;
    switch (m_port.error()) {
    case QSerialPort::DeviceNotFoundError:
        return QStringLiteral("Serial device not found: %1").arg(name);
    case QSerialPort::PermissionError:
        return QStringLiteral("Permission denied opening %1. Add this user to the dialout group and log in again.")
            .arg(name);
    default:
        break;
    }

    const QString detail = m_port.errorString();
    if (detail.isEmpty())
        return QStringLiteral("Could not open %1").arg(name);
    return QStringLiteral("%1 (%2)").arg(detail, name);
}

void SerialReader::onReadyRead()
{
    const LineBuffer::FeedResult fed = m_lines.feed(m_port.readAll());
    if (fed.overflow)
        emit malformedLine(QStringLiteral("Discarded an oversized or unterminated serial line"));

    for (const QByteArray &line : fed.lines) {
        const ParseResult parsed = parseSensorLine(line);
        if (parsed.status == ParseStatus::Empty || parsed.status == ParseStatus::Header)
            continue;
        if (parsed.status != ParseStatus::Ok) {
            emit malformedLine(parsed.error);
            continue;
        }
        emit sampleReceived(parsed.sample);
    }
}

void SerialReader::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || m_closing || !m_port.isOpen())
        return;

    const QString detail = m_port.errorString();
    const QString message = detail.isEmpty()
        ? QStringLiteral("Serial device disconnected")
        : QStringLiteral("Serial device disconnected (%1)").arg(detail);

    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        m_closing = true;
        m_port.close();
        m_closing = false;
        m_lines.clear();
        emit closed(message);
    }
}
