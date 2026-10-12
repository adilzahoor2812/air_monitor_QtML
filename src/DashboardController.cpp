#include "DashboardController.h"

#include <QDateTime>
#include <QDir>
#include <QDebug>

namespace
{
constexpr qint64 kChartWindowMs = 20000;
constexpr int kMaxChartPoints = 1200;
constexpr qint64 kLiveTimeoutMs = 2000;
}

DashboardController::DashboardController(const QString &portName, int baudRate, QObject *parent)
    : QObject(parent)
    , m_portName(portName)
    , m_baudRate(baudRate)
{
    connect(&m_serial, &SerialReader::opened, this, [this]() {
        m_connected = true;
        m_lastError.clear();
        emit connectionChanged();
    });
    connect(&m_serial, &SerialReader::closed, this, [this](const QString &reason) {
        m_connected = false;
        if (m_mockMode)
            return;
        if (!reason.isEmpty()) {
            m_lastError = reason;
            qWarning().noquote() << "Serial:" << reason;
        }
        emit connectionChanged();
    });
    connect(&m_serial, &SerialReader::malformedLine, this, [this](const QString &reason) {
        ++m_parseErrorCount;
        m_lastParseError = reason;
        m_statsDirty = true;
    });
    connect(&m_serial, &SerialReader::sampleReceived, this, [this](const SensorSample &sample) {
        ingest(sample, false);
    });
    connect(&m_mock, &MockGenerator::sampleReceived, this, [this](const SensorSample &sample) {
        ingest(sample, true);
    });

    m_uiTimer.setInterval(50);
    m_uiTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_uiTimer, &QTimer::timeout, this, &DashboardController::onUiTick);
    m_uiTimer.start();
}

bool DashboardController::connected() const { return m_connected; }
bool DashboardController::mockMode() const { return m_mockMode; }
bool DashboardController::live() const { return m_live; }
bool DashboardController::recording() const { return m_recorder.isRecording(); }
QString DashboardController::portName() const { return m_portName; }
int DashboardController::baudRate() const { return m_baudRate; }
QString DashboardController::lastError() const { return m_lastError; }
QString DashboardController::lastParseError() const { return m_lastParseError; }
int DashboardController::parseErrorCount() const { return m_parseErrorCount; }
int DashboardController::sampleCount() const { return m_sampleCount; }
double DashboardController::samplesPerSecond() const { return m_samplesPerSecond; }
bool DashboardController::hasSample() const { return m_hasSample; }
qint64 DashboardController::timestampMs() const { return m_timestampMs; }
double DashboardController::distanceCm() const { return m_distanceCm; }
double DashboardController::ax() const { return m_ax; }
double DashboardController::ay() const { return m_ay; }
double DashboardController::az() const { return m_az; }
double DashboardController::gx() const { return m_gx; }
double DashboardController::gy() const { return m_gy; }
double DashboardController::gz() const { return m_gz; }
QString DashboardController::state() const { return m_state; }
QString DashboardController::recordingPath() const { return m_recorder.path(); }
int DashboardController::recordedSamples() const { return m_recorder.rowCount(); }

QString DashboardController::statusText() const
{
    if (m_mockMode)
        return m_live ? QStringLiteral("Mock data") : QStringLiteral("Mock starting");
    if (!m_connected)
        return QStringLiteral("Disconnected");
    if (!m_live)
        return QStringLiteral("Connected, no data");
    return QStringLiteral("Connected");
}

QString DashboardController::statusLevel() const
{
    if (m_mockMode)
        return QStringLiteral("mock");
    if (!m_connected)
        return m_lastError.isEmpty() ? QStringLiteral("idle") : QStringLiteral("error");
    if (!m_live)
        return QStringLiteral("warn");
    return QStringLiteral("ok");
}

void DashboardController::connectPort()
{
    if (m_mockMode) {
        m_lastError = QStringLiteral("Turn off mock data before connecting to the serial port");
        emit connectionChanged();
        return;
    }
    if (m_serial.isOpen())
        return;

    if (!m_serial.open(m_portName, m_baudRate)) {
        m_connected = false;
        m_lastError = m_serial.lastError();
        qWarning().noquote() << "Serial:" << m_lastError;
        emit connectionChanged();
    }
}

void DashboardController::disconnectPort()
{
    m_lastError.clear();
    m_serial.close();
    if (!m_serial.isOpen() && m_connected) {
        m_connected = false;
        emit connectionChanged();
    }
}

void DashboardController::setMockMode(bool enabled)
{
    if (m_mockMode == enabled)
        return;

    if (enabled) {
        m_mockMode = true;
        m_lastError.clear();
        m_serial.close();
        m_connected = false;
        m_mock.setRunning(true);
    } else {
        m_mock.setRunning(false);
        m_mockMode = false;
    }
    emit connectionChanged();
}

void DashboardController::toggleRecording()
{
    if (m_recorder.isRecording()) {
        m_recorder.stop();
        emit recordingChanged();
        return;
    }

    const QString directory = QDir::homePath() + QStringLiteral("/SensorDashboard/recordings");
    if (!m_recorder.start(directory)) {
        m_lastError = m_recorder.lastError();
        qWarning().noquote() << "Recording:" << m_lastError;
        emit connectionChanged();
        emit recordingChanged();
        return;
    }
    emit recordingChanged();
}

QVariantList DashboardController::series(const QString &name) const
{
    double probe = 0.0;
    if (m_chart.empty() || !chartValue(m_chart.front(), name, &probe))
        return {};

    QVariantList values;
    values.reserve(static_cast<int>(m_chart.size()));
    for (const ChartPoint &point : m_chart) {
        double value = 0.0;
        chartValue(point, name, &value);
        values.append(value);
    }
    return values;
}

QVariantList DashboardController::timestamps() const
{
    QVariantList stamps;
    stamps.reserve(static_cast<int>(m_chart.size()));
    for (const ChartPoint &point : m_chart)
        stamps.append(static_cast<double>(point.arrivalMs));
    return stamps;
}

void DashboardController::ingest(const SensorSample &sample, bool fromMock)
{
    if (fromMock != m_mockMode || !sample.valid)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_hasSample = true;
    m_timestampMs = sample.timestampMs;
    m_distanceCm = sample.distanceCm;
    m_ax = sample.ax;
    m_ay = sample.ay;
    m_az = sample.az;
    m_gx = sample.gx;
    m_gy = sample.gy;
    m_gz = sample.gz;
    m_state = sample.state;
    ++m_sampleCount;
    m_lastArrivalMs = now;
    m_arrivals.push_back(now);
    appendChart(sample, now);

    if (m_recorder.isRecording() && !m_recorder.write(sample)) {
        m_lastError = m_recorder.lastError();
        qWarning().noquote() << "Recording:" << m_lastError;
        emit connectionChanged();
        emit recordingChanged();
    }

    m_dirty = true;
}

void DashboardController::appendChart(const SensorSample &sample, qint64 arrivalMs)
{
    ChartPoint point;
    point.arrivalMs = arrivalMs;
    point.distanceCm = sample.distanceCm;
    point.ax = sample.ax;
    point.ay = sample.ay;
    point.az = sample.az;
    point.gx = sample.gx;
    point.gy = sample.gy;
    point.gz = sample.gz;
    m_chart.push_back(point);

    const qint64 cutoff = arrivalMs - kChartWindowMs;
    while (!m_chart.empty() && m_chart.front().arrivalMs < cutoff)
        m_chart.pop_front();
    while (static_cast<int>(m_chart.size()) > kMaxChartPoints)
        m_chart.pop_front();
}

void DashboardController::onUiTick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_arrivals.empty() && now - m_arrivals.front() > 1000)
        m_arrivals.pop_front();

    const double samplesPerSecond = static_cast<double>(m_arrivals.size());
    const bool liveNow = computeLive(now);
    const bool rateChanged = samplesPerSecond != m_samplesPerSecond;
    const bool liveChanged = liveNow != m_live;
    if (rateChanged)
        m_samplesPerSecond = samplesPerSecond;
    if (liveChanged)
        m_live = liveNow;

    if (m_dirty) {
        m_dirty = false;
        emit sampleChanged();
        emit historyChanged();
    }
    if (rateChanged || m_statsDirty || m_recorder.isRecording()) {
        m_statsDirty = false;
        emit statsChanged();
    }
    if (liveChanged)
        emit connectionChanged();
}

bool DashboardController::computeLive(qint64 nowMs) const
{
    if (!(m_connected || m_mockMode) || !m_hasSample || m_lastArrivalMs == 0)
        return false;
    return nowMs - m_lastArrivalMs < kLiveTimeoutMs;
}

bool DashboardController::chartValue(const ChartPoint &point, const QString &name, double *value)
{
    if (name == QLatin1String("distance"))
        *value = point.distanceCm;
    else if (name == QLatin1String("ax"))
        *value = point.ax;
    else if (name == QLatin1String("ay"))
        *value = point.ay;
    else if (name == QLatin1String("az"))
        *value = point.az;
    else if (name == QLatin1String("gx"))
        *value = point.gx;
    else if (name == QLatin1String("gy"))
        *value = point.gy;
    else if (name == QLatin1String("gz"))
        *value = point.gz;
    else
        return false;
    return true;
}
