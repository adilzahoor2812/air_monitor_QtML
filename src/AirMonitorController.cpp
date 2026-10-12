#include "AirMonitorController.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtMath>

#include <algorithm>

namespace
{
constexpr int kSparkPoints = 24;

QString normalizeZone(QString zone)
{
    zone = zone.trimmed().toUpper();
    if (zone == QLatin1String("OK") || zone == QLatin1String("SAFE") || zone == QLatin1String("BEREIT"))
        return QStringLiteral("SAFE");
    if (zone == QLatin1String("WARN") || zone == QLatin1String("WARNING") || zone == QLatin1String("WARNUNG"))
        return QStringLiteral("WARN");
    if (zone == QLatin1String("FAULT") || zone == QLatin1String("ALARM") || zone == QLatin1String("STORUNG")
        || zone == QLatin1String("STÖRUNG") || zone == QLatin1String("ERROR"))
        return QStringLiteral("FAULT");
    return zone;
}

int severityFromZone(const QString &zone)
{
    if (zone == QLatin1String("FAULT"))
        return 2;
    if (zone == QLatin1String("WARN"))
        return 1;
    return 0;
}
}

AirMonitorController::AirMonitorController(QString host, quint16 port, bool liveMode, QObject *parent)
    : QObject(parent)
    , m_host(std::move(host))
    , m_port(port)
    , m_liveMode(liveMode)
{
    m_distSpark.reserve(kSparkPoints);
    for (int i = 0; i < kSparkPoints; ++i)
        m_distSpark.append(0);

    connect(&m_socket, &QTcpSocket::connected, this, &AirMonitorController::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &AirMonitorController::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &AirMonitorController::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &AirMonitorController::onSocketError);

    // STM32 often emits one JSON line per TCP session, then goes quiet while
    // keeping the socket ESTABLISHED. Poll frequently so we can reconnect.
    m_reconnect.setInterval(200);
    connect(&m_reconnect, &QTimer::timeout, this, &AirMonitorController::onReconnectTick);

    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &AirMonitorController::onClockTick);
    m_clock.start();
    onClockTick();

    // Keep UI responsive: parse bursts without blocking paints, publish ~20 Hz.
    m_uiTimer.setInterval(50);
    m_uiTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_uiTimer, &QTimer::timeout, this, &AirMonitorController::onUiTick);
    m_uiTimer.start();

    if (m_liveMode) {
        clearLiveHold();
        m_events = QVariantList();
        refreshTexts();
        connectSocket();
        m_reconnect.start();
    } else {
        applyDemoDefaults();
        refreshTexts();
    }
}

QString AirMonitorController::connectionText() const
{
    if (!m_liveMode)
        return i18n(QStringLiteral("DEMO"), QStringLiteral("DEMO"));
    if (m_linkEth)
        return i18n(QStringLiteral("LIVE"), QStringLiteral("LIVE"));
    if (!m_lastError.isEmpty())
        return m_lastError;
    return i18n(QStringLiteral("OFFLINE"), QStringLiteral("OFFLINE"));
}

void AirMonitorController::setLangEn(bool enabled)
{
    if (m_langEn == enabled)
        return;
    m_langEn = enabled;
    refreshTexts();
    emit langChanged();
    emit sampleChanged();
    emit connectionChanged();
    emit pageChanged();
    emit eventsChanged();
    emit uiChanged();
}

void AirMonitorController::setPage(int page)
{
    if (m_page == page)
        return;
    m_page = page;
    emit pageChanged();
    emit sampleChanged(); // pageTitle depends on page + language
}

void AirMonitorController::setLogOpen(bool open)
{
    if (m_logOpen == open)
        return;
    m_logOpen = open;
    emit uiChanged();
}

void AirMonitorController::setAckOpen(bool open)
{
    if (m_ackOpen == open)
        return;
    m_ackOpen = open;
    emit uiChanged();
}

QString AirMonitorController::i18n(const QString &de, const QString &en) const
{
    return m_langEn ? en : de;
}

QString AirMonitorController::faultDisplay() const
{
    if (m_faultKey == QLatin1String("temp-alarm"))
        return i18n(QStringLiteral("Temperatur Alarm"), QStringLiteral("Temperature alarm"));
    if (m_faultKey == QLatin1String("temp-warn"))
        return i18n(QStringLiteral("Temperatur Warnung"), QStringLiteral("Temperature warning"));
    if (m_faultKey == QLatin1String("hum-alarm"))
        return i18n(QStringLiteral("Feuchte Alarm"), QStringLiteral("Humidity alarm"));
    if (m_faultKey == QLatin1String("hum-warn"))
        return i18n(QStringLiteral("Feuchte Warnung"), QStringLiteral("Humidity warning"));
    if (m_faultKey == QLatin1String("dist-alarm"))
        return i18n(QStringLiteral("Distanz Alarm"), QStringLiteral("Distance alarm"));
    if (m_faultKey == QLatin1String("dist-warn"))
        return i18n(QStringLiteral("Distanz Warnung"), QStringLiteral("Distance warning"));
    if (!m_faultText.isEmpty())
        return m_faultText;
    return i18n(QStringLiteral("keine"), QStringLiteral("none"));
}

QString AirMonitorController::stateWord() const
{
    if (m_liveMode && !m_hasSample)
        return QStringLiteral("\u2014");
    if (m_severity <= 0)
        return i18n(QStringLiteral("BEREIT"), QStringLiteral("READY"));
    if (m_alarmAcked)
        return i18n(QStringLiteral("QUITT."), QStringLiteral("ACK'd"));
    if (m_severity == 1)
        return i18n(QStringLiteral("WARNUNG"), QStringLiteral("WARNING"));
    return i18n(QStringLiteral("STÖRUNG"), QStringLiteral("FAULT"));
}

QString AirMonitorController::headerWord() const
{
    if (m_liveMode && !m_linkEth)
        return i18n(QStringLiteral("OFFLINE"), QStringLiteral("OFFLINE"));
    if (m_severity >= 2)
        return i18n(QStringLiteral("STÖRUNG"), QStringLiteral("FAULT"));
    if (m_severity == 1)
        return i18n(QStringLiteral("WARNUNG"), QStringLiteral("WARNING"));
    return i18n(QStringLiteral("BETRIEB"), QStringLiteral("OPERATION"));
}

QString AirMonitorController::pageTitle() const
{
    if (m_page == 1)
        return i18n(QStringLiteral("Kalibrierung"), QStringLiteral("Calibration"));
    if (m_page == 3)
        return i18n(QStringLiteral("Alarm"), QStringLiteral("Alarm"));
    return i18n(QStringLiteral("Betriebsübersicht"), QStringLiteral("Operation Overview"));
}

void AirMonitorController::acknowledge()
{
    if (!(m_ackArmed || !m_linkNfc)) {
        m_ackFailed = true;
        m_ackKey = QStringLiteral("locked");
        m_ackOpen = true;
        refreshTexts();
        emit uiChanged();
        emit sampleChanged();
        return;
    }

    m_alarmAcked = true;
    m_ackFailed = false;
    m_ackKey = QStringLiteral("ok");
    m_ackOpen = true;
    refreshTexts();
    pushEvent(i18n(QStringLiteral("Quittiert"), QStringLiteral("Acknowledged")));
    emit uiChanged();
    emit sampleChanged();

    if (m_liveMode && m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.write("ACK\n");
}

void AirMonitorController::reconnect()
{
    if (!m_liveMode)
        return;
    m_lastError.clear();
    connectSocket();
    emit connectionChanged();
}

void AirMonitorController::applyDemoDefaults()
{
    m_hasSample = true;
    m_linkEth = true;
    m_tempC = 24.1;
    m_humPct = 48.0;
    m_distanceCm = 72;
    m_cpuPct = 18;
    m_heapFreePct = 62;
    m_linkNfc = false;
    m_ackArmed = false;
    m_distSpark = {52, 61, 74, 58, 80, 69, 88, 63, 77, 55, 82, 70, 91, 66, 73, 57, 84, 71, 79, 60, 86, 68, 75, 72};
    m_events = QVariantList{QVariantMap{{QStringLiteral("t"), QStringLiteral("20:45")},
                                        {QStringLiteral("text"), QStringLiteral("Sensor OK")}}};
    recomputeLocalSeverity();
}

void AirMonitorController::clearLiveHold()
{
    m_hasSample = false;
    m_linkEth = false;
    m_tempC = 0.0;
    m_humPct = 0.0;
    m_distanceCm = -1;
    m_cpuPct = 0;
    m_heapFreePct = 0;
    m_severity = 0;
    m_alarmAcked = false;
    m_faultKey.clear();
    m_faultText.clear();
    m_zone = QStringLiteral("SAFE");
    for (int i = 0; i < m_distSpark.size(); ++i)
        m_distSpark[i] = 0;
}

void AirMonitorController::recomputeLocalSeverity()
{
    int nextSev = 0;
    QString nextKey;
    auto consider = [&](int level, const char *key) {
        if (level > nextSev) {
            nextSev = level;
            nextKey = QLatin1String(key);
        }
    };

    if (m_tempC < 0.0 || m_tempC > 40.0)
        consider(2, "temp-alarm");
    else if (m_tempC < 5.0 || m_tempC > 32.0)
        consider(1, "temp-warn");

    if (m_humPct < 15.0 || m_humPct > 90.0)
        consider(2, "hum-alarm");
    else if (m_humPct < 25.0 || m_humPct > 75.0)
        consider(1, "hum-warn");

    if (m_distanceCm >= 0) {
        if (m_distanceCm < 20)
            consider(2, "dist-alarm");
        else if (m_distanceCm < 50)
            consider(1, "dist-warn");
    }

    m_severity = nextSev;
    m_faultKey = nextKey;
    m_zone = nextSev == 0 ? QStringLiteral("SAFE")
                          : (nextSev == 1 ? QStringLiteral("WARN") : QStringLiteral("FAULT"));
    if (nextSev == 0)
        m_alarmAcked = false;
}

void AirMonitorController::refreshTexts()
{
    m_faultText = faultDisplay();
    if (m_ackKey == QLatin1String("ok"))
        m_ackStatus = i18n(QStringLiteral("quittiert"), QStringLiteral("acknowledged"));
    else if (m_ackKey == QLatin1String("locked"))
        m_ackStatus = i18n(QStringLiteral("gesperrt"), QStringLiteral("locked"));
    else
        m_ackStatus.clear();

    if (m_liveMode && !m_linkEth)
        m_statusLine = i18n(QStringLiteral("Ethernet getrennt"), QStringLiteral("Ethernet offline"));
    else if (m_liveMode && !m_hasSample)
        m_statusLine = i18n(QStringLiteral("Warte auf STM32-Daten"), QStringLiteral("Waiting for STM32 data"));
    else if (m_severity <= 0)
        m_statusLine = i18n(QStringLiteral("Prozess im Sollbereich"), QStringLiteral("Process within range"));
    else if (m_alarmAcked)
        m_statusLine = i18n(QStringLiteral("Quittiert. Zustand bleibt bis der Prozess gesund ist."),
                          QStringLiteral("Acknowledged. Condition remains until the process is healthy."));
    else
        m_statusLine = m_faultText;
}

void AirMonitorController::pushEvent(const QString &text)
{
    QVariantList copy = m_events;
    copy.prepend(QVariantMap{{QStringLiteral("t"), QTime::currentTime().toString(QStringLiteral("HH:mm"))},
                             {QStringLiteral("text"), text}});
    while (copy.size() > 50)
        copy.removeLast();
    m_events = copy;
    emit eventsChanged();
}

void AirMonitorController::connectSocket()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState
        || m_socket.state() == QAbstractSocket::ConnectingState)
        return;
    m_rx.clear();
    m_lastSampleMs = 0;
    if (!m_quietReconnect)
        qInfo().noquote() << "Connecting to" << m_host << m_port;
    m_socket.connectToHost(m_host, m_port);
}

void AirMonitorController::refreshSocketQuiet()
{
    // STM32 often keeps ESTABLISHED but stops sending. Drop and reopen the
    // socket without toggling LIVE/OFFLINE in the UI.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastStaleReconnectMs < 120)
        return;
    m_lastStaleReconnectMs = now;
    m_quietReconnect = true;
    m_socket.abort();
    m_rx.clear();
    m_lastSampleMs = 0;
    connectSocket();
}

void AirMonitorController::onConnected()
{
    const bool quiet = m_quietReconnect;
    m_quietReconnect = false;
    m_linkEth = true;
    m_lastError.clear();
    m_rx.clear();
    m_samplesSeen = 0;
    // Arm stale watchdog from connect time so a silent socket is recovered.
    m_lastSampleMs = QDateTime::currentMSecsSinceEpoch();
    if (!quiet) {
        qInfo().noquote() << "TCP connected to" << m_host << m_port;
        pushEvent(i18n(QStringLiteral("Ethernet Verbunden"), QStringLiteral("Ethernet Connected")));
        m_connDirty = true;
    }
    m_uiDirty = true;
    publishUi(true);
}

void AirMonitorController::onDisconnected()
{
    // Intentional telemetry refresh — keep LIVE up; do not spam events.
    if (m_quietReconnect)
        return;

    const bool wasUp = m_linkEth;
    m_linkEth = false;
    if (wasUp)
        pushEvent(i18n(QStringLiteral("Ethernet Getrennt"), QStringLiteral("Ethernet Offline")));
    m_connDirty = true;
    m_uiDirty = true;
    publishUi(true);
}

void AirMonitorController::onReadyRead()
{
    m_rx.append(m_socket.readAll());

    // Do not parse an unbounded burst in one event-loop turn — that freezes QML paints.
    constexpr int kMaxLinesPerEvent = 6;
    int processed = 0;
    bool distanceJump = false;
    const int distanceBefore = m_distanceCm;

    while (processed < kMaxLinesPerEvent) {
        const int nl = m_rx.indexOf('\n');
        if (nl < 0) {
            if (m_rx.size() > 16384)
                m_rx.clear();
            break;
        }
        QByteArray line = m_rx.left(nl);
        m_rx.remove(0, nl + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty() || line.startsWith('#'))
            continue;
        if (parseTelemetryLine(line)) {
            ++m_samplesSeen;
            ++processed;
            m_lastSampleMs = QDateTime::currentMSecsSinceEpoch();
            if (m_samplesSeen <= 3 || (m_samplesSeen % 50) == 0)
                qInfo().noquote() << "Telemetry OK #" << m_samplesSeen << ":"
                                  << QString::fromUtf8(line);
            m_uiDirty = true;
            if (m_distanceCm != distanceBefore
                && (distanceBefore < 0 || m_distanceCm < 0
                    || qAbs(m_distanceCm - distanceBefore) >= 2)) {
                distanceJump = true;
                m_distanceDirty = true;
            }
        } else if (!line.trimmed().isEmpty()) {
            qWarning().noquote() << "Telemetry rejected:" << QString::fromUtf8(line.left(200));
        }
    }

    // Continue later if we capped this turn — never spin tightly in one burst.
    if (processed >= kMaxLinesPerEvent && m_rx.contains('\n') && !m_readContinuationPending) {
        m_readContinuationPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_readContinuationPending = false;
            onReadyRead();
        });
    }

    // Publish every parsed batch immediately — one-shot firmware sessions
    // must paint before the socket goes quiet again.
    if (m_uiDirty && (processed > 0 || distanceJump))
        publishUi(true);

    // Firmware usually stops after one line. Quietly reopen so values keep
    // updating without flashing Ethernet offline in the UI.
    if (processed > 0) {
        QTimer::singleShot(100, this, [this]() {
            if (!m_liveMode)
                return;
            if (m_socket.state() != QAbstractSocket::ConnectedState)
                return;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (m_lastSampleMs > 0 && now - m_lastSampleMs >= 100)
                refreshSocketQuiet();
        });
    }
}

void AirMonitorController::onSocketError(QAbstractSocket::SocketError)
{
    // Quiet refresh aborts are expected — ignore RemoteHostClosed/etc.
    if (m_quietReconnect)
        return;
    // Ignore transient errors while a live session is already up.
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        return;

    m_lastError = m_socket.errorString();
    // Only drop LIVE if we have not had a sample for a while.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool trulyDown = m_lastSampleMs == 0 || now - m_lastSampleMs > 3000;
    if (trulyDown && m_linkEth) {
        m_linkEth = false;
        qWarning().noquote() << "TCP error:" << m_lastError;
        refreshTexts();
        emit connectionChanged();
        emit sampleChanged();
    }
}

void AirMonitorController::onReconnectTick()
{
    if (!m_liveMode)
        return;

    const auto state = m_socket.state();
    if (state == QAbstractSocket::UnconnectedState) {
        connectSocket();
        return;
    }

    if (state != QAbstractSocket::ConnectedState)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Mark OFFLINE only after sustained silence (real cable/host loss).
    if (m_linkEth && m_lastSampleMs > 0 && now - m_lastSampleMs > 3000) {
        m_linkEth = false;
        m_connDirty = true;
        m_uiDirty = true;
        publishUi(true);
    }

    // Firmware typically pushes ~1 line then stalls — quiet refresh.
    if (m_lastSampleMs > 0 && now - m_lastSampleMs > 280)
        refreshSocketQuiet();
}

void AirMonitorController::onClockTick()
{
    m_clockText = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    emit clockChanged();
}

void AirMonitorController::onUiTick()
{
    if (m_uiDirty || m_connDirty)
        publishUi(false);
}

void AirMonitorController::publishUi(bool force)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && now - m_lastUiPublishMs < 40)
        return;

    m_lastUiPublishMs = now;
    m_uiDirty = false;
    const bool conn = m_connDirty;
    m_connDirty = false;

    // Update sparkline once per UI frame (not once per TCP line).
    if (m_distanceCm >= 0 && m_distanceCm != m_publishedDistanceCm) {
        QVariantList spark = m_distSpark;
        spark.removeFirst();
        spark.append(std::clamp(m_distanceCm, 0, 400));
        m_distSpark = spark;
    }

    refreshTexts();
    emit sampleChanged();
    if (m_distanceDirty || m_distanceCm != m_publishedDistanceCm) {
        m_publishedDistanceCm = m_distanceCm;
        m_distanceDirty = false;
        emit distanceChanged();
    }
    if (conn)
        emit connectionChanged();
}

bool AirMonitorController::parseTelemetryLine(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.startsWith('{'))
        return parseJsonTelemetry(trimmed);
    return parseCsvTelemetry(trimmed);
}

bool AirMonitorController::parseJsonTelemetry(const QByteArray &line)
{
    // STM32 F756 live stream example:
    // {"ok":1,"temp_c":22.0,"hum_pct":55.8,"distance_cm":-1,"cpu":1,"heap":99,
    //  "link_eth":1,"link_nfc":0,"link_f401":0,"freertos":1,"watchdog":1,"zone":"SAFE"}
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject o = doc.object();
    if (o.contains(QStringLiteral("ok")) && o.value(QStringLiteral("ok")).toInt(1) == 0)
        return false;

    const auto num = [&](const char *a, const char *b, bool *ok) -> double {
        if (o.contains(QLatin1String(a))) {
            *ok = true;
            return o.value(QLatin1String(a)).toDouble();
        }
        if (b && o.contains(QLatin1String(b))) {
            *ok = true;
            return o.value(QLatin1String(b)).toDouble();
        }
        *ok = false;
        return 0.0;
    };

    bool ok = false;
    const double temp = num("temp_c", "tempC", &ok);
    if (!ok || !qIsFinite(temp))
        return false;
    const double hum = num("hum_pct", "humPct", &ok);
    if (!ok || !qIsFinite(hum))
        return false;

    int distance = -1;
    if (o.contains(QStringLiteral("distance_cm")))
        distance = o.value(QStringLiteral("distance_cm")).toInt(-1);
    else if (o.contains(QStringLiteral("distanceCm")))
        distance = o.value(QStringLiteral("distanceCm")).toInt(-1);

    QString zone = QStringLiteral("SAFE");
    if (o.contains(QStringLiteral("zone")))
        zone = normalizeZone(o.value(QStringLiteral("zone")).toString());

    int cpu = 0;
    if (o.contains(QStringLiteral("cpu")))
        cpu = o.value(QStringLiteral("cpu")).toInt();
    else if (o.contains(QStringLiteral("cpu_pct")))
        cpu = o.value(QStringLiteral("cpu_pct")).toInt();
    else if (o.contains(QStringLiteral("cpuPct")))
        cpu = o.value(QStringLiteral("cpuPct")).toInt();

    int heap = 0;
    if (o.contains(QStringLiteral("heap")))
        heap = o.value(QStringLiteral("heap")).toInt();
    else if (o.contains(QStringLiteral("heap_free_pct")))
        heap = o.value(QStringLiteral("heap_free_pct")).toInt();
    else if (o.contains(QStringLiteral("heapFreePct")))
        heap = o.value(QStringLiteral("heapFreePct")).toInt();

    bool nfc = m_linkNfc;
    if (o.contains(QStringLiteral("link_nfc")))
        nfc = o.value(QStringLiteral("link_nfc")).toInt() != 0;
    else if (o.contains(QStringLiteral("linkNfc")))
        nfc = o.value(QStringLiteral("linkNfc")).toBool();

    bool armed = m_ackArmed;
    if (o.contains(QStringLiteral("ack_armed")))
        armed = o.value(QStringLiteral("ack_armed")).toInt() != 0;
    else if (o.contains(QStringLiteral("ackArmed")))
        armed = o.value(QStringLiteral("ackArmed")).toBool();

    QString event;
    if (o.contains(QStringLiteral("event")))
        event = o.value(QStringLiteral("event")).toString();

    applySample(temp, hum, distance, zone, cpu, heap, nfc, armed, event);
    return true;
}

bool AirMonitorController::parseCsvTelemetry(const QByteArray &line)
{
    // CSV fallback:
    // temp_c,hum_pct,distance_cm,zone,cpu_pct,heap_free_pct,nfc,ack_armed[,event]
    const QList<QByteArray> parts = line.split(',');
    if (parts.size() < 6)
        return false;

    int idx = 0;
    bool ok = false;
    if (parts.size() >= 8) {
        parts.at(0).trimmed().toLongLong(&ok);
        if (ok)
            idx = 1;
    }
    if (parts.size() - idx < 6)
        return false;

    const double temp = parts.at(idx).trimmed().toDouble(&ok);
    if (!ok || !qIsFinite(temp))
        return false;
    const double hum = parts.at(idx + 1).trimmed().toDouble(&ok);
    if (!ok || !qIsFinite(hum))
        return false;

    const QByteArray distField = parts.at(idx + 2).trimmed();
    int distance = -1;
    if (distField != "-" && distField != "\xE2\x80\x94") {
        distance = distField.toInt(&ok);
        if (!ok)
            return false;
    }

    const QString zone = normalizeZone(QString::fromLatin1(parts.at(idx + 3).trimmed()));
    const int cpu = parts.at(idx + 4).trimmed().toInt(&ok);
    if (!ok)
        return false;
    const int heap = parts.at(idx + 5).trimmed().toInt(&ok);
    if (!ok)
        return false;

    bool nfc = m_linkNfc;
    bool armed = m_ackArmed;
    QString event;
    if (parts.size() - idx >= 7)
        nfc = parts.at(idx + 6).trimmed().toInt(&ok) != 0 && ok;
    if (parts.size() - idx >= 8)
        armed = parts.at(idx + 7).trimmed().toInt(&ok) != 0 && ok;
    if (parts.size() - idx >= 9)
        event = QString::fromUtf8(parts.at(idx + 8).trimmed());

    applySample(temp, hum, distance, zone, cpu, heap, nfc, armed, event);
    return true;
}

void AirMonitorController::applySample(double tempC, double humPct, int distanceCm, const QString &zone,
                                       int cpuPct, int heapFreePct, bool linkNfc, bool ackArmed,
                                       const QString &event)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_tempC = tempC;
    m_humPct = humPct;
    // HC-SR04 streams sometimes emit -1 on a missed echo. Keep the last good
    // reading briefly so the UI does not freeze on "—" while the sensor moves.
    if (distanceCm >= 0) {
        if (m_distanceCm != distanceCm)
            m_distanceDirty = true;
        m_distanceCm = distanceCm;
        m_lastGoodDistanceMs = now;
    } else if (m_lastGoodDistanceMs == 0 || now - m_lastGoodDistanceMs > 400) {
        if (m_distanceCm != -1)
            m_distanceDirty = true;
        m_distanceCm = -1;
    }
    m_cpuPct = std::clamp(cpuPct, 0, 100);
    m_heapFreePct = std::clamp(heapFreePct, 0, 100);
    m_linkNfc = linkNfc;
    m_ackArmed = ackArmed;
    m_hasSample = true;
    m_linkEth = true;

    const QString z = normalizeZone(zone);
    if (z == QLatin1String("SAFE") || z == QLatin1String("WARN") || z == QLatin1String("FAULT")) {
        m_zone = z;
        m_severity = severityFromZone(z);
        m_faultKey.clear();
        if (m_severity == 0)
            m_alarmAcked = false;
    } else {
        recomputeLocalSeverity();
    }

    if (!event.isEmpty())
        pushEvent(event);
}
