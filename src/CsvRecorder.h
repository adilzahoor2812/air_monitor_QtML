#pragma once

#include "SensorSample.h"

#include <QFile>
#include <QObject>
#include <QString>

class CsvRecorder : public QObject
{
    Q_OBJECT

public:
    explicit CsvRecorder(QObject *parent = nullptr);

    bool start(const QString &directory);
    void stop();
    bool write(const SensorSample &sample);
    bool isRecording() const;
    QString path() const;
    int rowCount() const;
    QString lastError() const;

private:
    QFile m_file;
    QString m_path;
    QString m_error;
    int m_rows = 0;
};
