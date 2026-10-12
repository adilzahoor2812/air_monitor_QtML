#pragma once

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantList>

class AirMonitorController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal tempC READ tempC NOTIFY sampleChanged)
    Q_PROPERTY(qreal humPct READ humPct NOTIFY sampleChanged)
    Q_PROPERTY(int distanceCm READ distanceCm NOTIFY distanceChanged)
    Q_PROPERTY(QVariantList distSpark READ distSpark NOTIFY sampleChanged)
    Q_PROPERTY(int cpuPct READ cpuPct NOTIFY sampleChanged)
    Q_PROPERTY(int heapFreePct READ heapFreePct NOTIFY sampleChanged)
    Q_PROPERTY(bool linkEth READ linkEth NOTIFY connectionChanged)
    Q_PROPERTY(bool linkNfc READ linkNfc NOTIFY sampleChanged)
    Q_PROPERTY(bool ackArmed READ ackArmed NOTIFY sampleChanged)
    Q_PROPERTY(bool langEn READ langEn WRITE setLangEn NOTIFY langChanged)
    Q_PROPERTY(QString ackStatus READ ackStatus NOTIFY sampleChanged)
    Q_PROPERTY(int severity READ severity NOTIFY sampleChanged)
    Q_PROPERTY(bool alarmAcked READ alarmAcked NOTIFY sampleChanged)
    Q_PROPERTY(QString faultText READ faultText NOTIFY sampleChanged)
    Q_PROPERTY(QString zone READ zone NOTIFY sampleChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY sampleChanged)
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(bool logOpen READ logOpen WRITE setLogOpen NOTIFY uiChanged)
    Q_PROPERTY(bool ackOpen READ ackOpen WRITE setAckOpen NOTIFY uiChanged)
    Q_PROPERTY(bool ackFailed READ ackFailed NOTIFY uiChanged)
    Q_PROPERTY(QString clockText READ clockText NOTIFY clockChanged)
    Q_PROPERTY(QString faultKey READ faultKey NOTIFY sampleChanged)
    Q_PROPERTY(QString ackKey READ ackKey NOTIFY sampleChanged)
    Q_PROPERTY(int releaseSeconds READ releaseSeconds NOTIFY sampleChanged)
    Q_PROPERTY(bool liveMode READ liveMode NOTIFY connectionChanged)
    Q_PROPERTY(bool hasSample READ hasSample NOTIFY sampleChanged)
    Q_PROPERTY(QString host READ host CONSTANT)
    Q_PROPERTY(int port READ port CONSTANT)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY connectionChanged)
    Q_PROPERTY(QString pageTitle READ pageTitle NOTIFY sampleChanged)
    Q_PROPERTY(QString headerWord READ headerWord NOTIFY sampleChanged)
    Q_PROPERTY(QString stateWord READ stateWord NOTIFY sampleChanged)
    Q_PROPERTY(QString faultDisplay READ faultDisplay NOTIFY sampleChanged)

public:
    explicit AirMonitorController(QString host, quint16 port, bool liveMode, QObject *parent = nullptr);

    qreal tempC() const { return m_tempC; }
    qreal humPct() const { return m_humPct; }
    int distanceCm() const { return m_distanceCm; }
    QVariantList distSpark() const { return m_distSpark; }
    int cpuPct() const { return m_cpuPct; }
    int heapFreePct() const { return m_heapFreePct; }
    bool linkEth() const { return m_linkEth; }
    bool linkNfc() const { return m_linkNfc; }
    bool ackArmed() const { return m_ackArmed; }
    bool langEn() const { return m_langEn; }
    QString ackStatus() const { return m_ackStatus; }
    int severity() const { return m_severity; }
    bool alarmAcked() const { return m_alarmAcked; }
    QString faultText() const { return m_faultText; }
    QString zone() const { return m_zone; }
    QString statusLine() const { return m_statusLine; }
    QVariantList events() const { return m_events; }
    int page() const { return m_page; }
    bool logOpen() const { return m_logOpen; }
    bool ackOpen() const { return m_ackOpen; }
    bool ackFailed() const { return m_ackFailed; }
    QString clockText() const { return m_clockText; }
    QString faultKey() const { return m_faultKey; }
    QString ackKey() const { return m_ackKey; }
    int releaseSeconds() const { return m_releaseSeconds; }
    bool liveMode() const { return m_liveMode; }
    bool hasSample() const { return m_hasSample; }
    QString host() const { return m_host; }
    int port() const { return m_port; }
    QString connectionText() const;

    void setLangEn(bool enabled);
    void setPage(int page);
    void setLogOpen(bool open);
    void setAckOpen(bool open);

    // Named i18n (not tr) so it does not collide with QObject::tr.
    Q_INVOKABLE QString i18n(const QString &de, const QString &en) const;
    QString faultDisplay() const;
    QString stateWord() const;
    QString headerWord() const;
    QString pageTitle() const;
    Q_INVOKABLE void acknowledge();
    Q_INVOKABLE void reconnect();

signals:
    void sampleChanged();
    void distanceChanged();
    void connectionChanged();
    void langChanged();
    void eventsChanged();
    void pageChanged();
    void uiChanged();
    void clockChanged();

private:
    void applyDemoDefaults();
    void clearLiveHold();
    void recomputeLocalSeverity();
    void refreshTexts();
    void pushEvent(const QString &text);
    void connectSocket();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTick();
    void onClockTick();
    void onUiTick();
    bool parseTelemetryLine(const QByteArray &line);
    bool parseJsonTelemetry(const QByteArray &line);
    bool parseCsvTelemetry(const QByteArray &line);
    void applySample(double tempC, double humPct, int distanceCm, const QString &zone,
                     int cpuPct, int heapFreePct, bool linkNfc, bool ackArmed,
                     const QString &event);
    void publishUi(bool force = false);

    QString m_host;
    quint16 m_port = 5000;
    bool m_liveMode = false;
    bool m_hasSample = false;
    bool m_linkEth = false;

    QTcpSocket m_socket;
    QByteArray m_rx;
    QTimer m_reconnect;
    QTimer m_clock;
    QTimer m_uiTimer;
    bool m_uiDirty = false;
    bool m_connDirty = false;
    qint64 m_lastUiPublishMs = 0;
    qint64 m_lastGoodDistanceMs = 0;
    qint64 m_lastSampleMs = 0;
    qint64 m_lastStaleReconnectMs = 0;
    int m_samplesSeen = 0;
    int m_publishedDistanceCm = -9999;
    bool m_distanceDirty = false;
    bool m_readContinuationPending = false;
    bool m_quietReconnect = false; // intentional refresh — do not flash OFFLINE
    void refreshSocketQuiet();

    double m_tempC = 0.0;
    double m_humPct = 0.0;
    int m_distanceCm = -1;
    QVariantList m_distSpark;
    int m_cpuPct = 0;
    int m_heapFreePct = 0;
    bool m_linkNfc = false;
    bool m_ackArmed = false;
    bool m_langEn = false;
    QString m_ackStatus;
    int m_severity = 0;
    bool m_alarmAcked = false;
    QString m_faultText;
    QString m_zone = QStringLiteral("SAFE");
    QString m_statusLine;
    QVariantList m_events;
    int m_page = 0;
    bool m_logOpen = false;
    bool m_ackOpen = false;
    bool m_ackFailed = false;
    QString m_clockText = QStringLiteral("00:00:00");
    QString m_faultKey;
    QString m_ackKey;
    int m_releaseSeconds = 30;
    QString m_lastError;
};
