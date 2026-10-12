#pragma once

#include "CsvRecorder.h"
#include "MockGenerator.h"
#include "SensorSample.h"
#include "SerialReader.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <deque>

class DashboardController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool mockMode READ mockMode WRITE setMockMode NOTIFY connectionChanged)
    Q_PROPERTY(bool live READ live NOTIFY connectionChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY connectionChanged)
    Q_PROPERTY(QString statusLevel READ statusLevel NOTIFY connectionChanged)
    Q_PROPERTY(QString portName READ portName CONSTANT)
    Q_PROPERTY(int baudRate READ baudRate CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY connectionChanged)
    Q_PROPERTY(QString lastParseError READ lastParseError NOTIFY statsChanged)
    Q_PROPERTY(int parseErrorCount READ parseErrorCount NOTIFY statsChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY statsChanged)
    Q_PROPERTY(double samplesPerSecond READ samplesPerSecond NOTIFY statsChanged)
    Q_PROPERTY(bool hasSample READ hasSample NOTIFY sampleChanged)
    Q_PROPERTY(qint64 timestampMs READ timestampMs NOTIFY sampleChanged)
    Q_PROPERTY(double distanceCm READ distanceCm NOTIFY sampleChanged)
    Q_PROPERTY(double ax READ ax NOTIFY sampleChanged)
    Q_PROPERTY(double ay READ ay NOTIFY sampleChanged)
    Q_PROPERTY(double az READ az NOTIFY sampleChanged)
    Q_PROPERTY(double gx READ gx NOTIFY sampleChanged)
    Q_PROPERTY(double gy READ gy NOTIFY sampleChanged)
    Q_PROPERTY(double gz READ gz NOTIFY sampleChanged)
    Q_PROPERTY(QString state READ state NOTIFY sampleChanged)
    Q_PROPERTY(QString recordingPath READ recordingPath NOTIFY recordingChanged)
    Q_PROPERTY(int recordedSamples READ recordedSamples NOTIFY statsChanged)

public:
    explicit DashboardController(const QString &portName = QStringLiteral("/dev/ttyACM0"),
                                 int baudRate = 115200,
                                 QObject *parent = nullptr);

    bool connected() const;
    bool mockMode() const;
    bool live() const;
    bool recording() const;
    QString statusText() const;
    QString statusLevel() const;
    QString portName() const;
    int baudRate() const;
    QString lastError() const;
    QString lastParseError() const;
    int parseErrorCount() const;
    int sampleCount() const;
    double samplesPerSecond() const;
    bool hasSample() const;
    qint64 timestampMs() const;
    double distanceCm() const;
    double ax() const;
    double ay() const;
    double az() const;
    double gx() const;
    double gy() const;
    double gz() const;
    QString state() const;
    QString recordingPath() const;
    int recordedSamples() const;

    Q_INVOKABLE void connectPort();
    Q_INVOKABLE void disconnectPort();
    void setMockMode(bool enabled);
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE QVariantList series(const QString &name) const;
    Q_INVOKABLE QVariantList timestamps() const;

signals:
    void connectionChanged();
    void sampleChanged();
    void recordingChanged();
    void statsChanged();
    void historyChanged();

private:
    struct ChartPoint
    {
        qint64 arrivalMs = 0;
        double distanceCm = 0.0;
        double ax = 0.0;
        double ay = 0.0;
        double az = 0.0;
        double gx = 0.0;
        double gy = 0.0;
        double gz = 0.0;
    };

    void ingest(const SensorSample &sample, bool fromMock);
    void appendChart(const SensorSample &sample, qint64 arrivalMs);
    void onUiTick();
    bool computeLive(qint64 nowMs) const;
    static bool chartValue(const ChartPoint &point, const QString &name, double *value);

    SerialReader m_serial;
    MockGenerator m_mock;
    CsvRecorder m_recorder;
    QTimer m_uiTimer;

    QString m_portName;
    int m_baudRate = 115200;
    bool m_connected = false;
    bool m_mockMode = false;
    bool m_live = false;
    bool m_dirty = false;
    bool m_statsDirty = false;
    bool m_hasSample = false;

    QString m_lastError;
    QString m_lastParseError;
    int m_parseErrorCount = 0;
    int m_sampleCount = 0;
    double m_samplesPerSecond = 0.0;

    qint64 m_timestampMs = 0;
    qint64 m_lastArrivalMs = 0;
    double m_distanceCm = 0.0;
    double m_ax = 0.0;
    double m_ay = 0.0;
    double m_az = 0.0;
    double m_gx = 0.0;
    double m_gy = 0.0;
    double m_gz = 0.0;
    QString m_state = QStringLiteral("UNKNOWN");

    std::deque<ChartPoint> m_chart;
    std::deque<qint64> m_arrivals;
};
