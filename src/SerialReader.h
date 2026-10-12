#pragma once

#include "LineBuffer.h"
#include "SensorSample.h"

#include <QObject>
#include <QSerialPort>
#include <QString>

class SerialReader : public QObject
{
    Q_OBJECT

public:
    explicit SerialReader(QObject *parent = nullptr);

    bool open(const QString &portName, int baudRate);
    void close();
    bool isOpen() const;
    QString lastError() const;

signals:
    void opened();
    void closed(const QString &reason);
    void sampleReceived(const SensorSample &sample);
    void malformedLine(const QString &reason);

private:
    QString describeOpenError() const;
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

    QSerialPort m_port;
    LineBuffer m_lines;
    QString m_requestedName;
    QString m_lastError;
    bool m_closing = false;
};
