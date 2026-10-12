import QtQuick

Item {
    id: vm

    width: 0
    height: 0
    visible: false

    property real tempC: 24.1
    property real humPct: 48.0
    property int distanceCm: 72
    property var distSpark: [52, 61, 74, 58, 80, 69, 88, 63, 77, 55, 82, 70, 91, 66, 73, 57, 84, 71, 79, 60, 86, 68, 75, 72]
    property int cpuPct: 18
    property int heapFreePct: 62
    property bool linkEth: true
    property bool linkNfc: false
    property bool ackArmed: false
    property bool langEn: false
    property string ackStatus: ""
    property int severity: 0
    property bool alarmAcked: false
    property string faultText: ""
    property string zone: "SAFE"
    property string statusLine: ""
    property var events: [{ t: "20:45", text: "Sensor OK" }]

    property int page: 0
    property bool logOpen: false
    property bool ackOpen: false
    property bool ackFailed: false
    property string clockText: "00:00:00"
    property string faultKey: ""
    property string ackKey: ""
    property int releaseSeconds: 30

    function tr(de, en) {
        return langEn ? en : de
    }

    function faultDisplay() {
        if (faultKey === "temp-alarm")
            return tr("Temperatur Alarm", "Temperature alarm")
        if (faultKey === "temp-warn")
            return tr("Temperatur Warnung", "Temperature warning")
        if (faultKey === "hum-alarm")
            return tr("Feuchte Alarm", "Humidity alarm")
        if (faultKey === "hum-warn")
            return tr("Feuchte Warnung", "Humidity warning")
        if (faultKey === "dist-alarm")
            return tr("Distanz Alarm", "Distance alarm")
        if (faultKey === "dist-warn")
            return tr("Distanz Warnung", "Distance warning")
        return tr("keine", "none")
    }

    function stateWord() {
        if (severity <= 0)
            return tr("BEREIT", "READY")
        if (alarmAcked)
            return tr("QUITT.", "ACK'd")
        if (severity === 1)
            return tr("WARNUNG", "WARNING")
        return tr("STÖRUNG", "FAULT")
    }

    function headerWord() {
        if (severity >= 2)
            return tr("STÖRUNG", "FAULT")
        if (severity === 1)
            return tr("WARNUNG", "WARNING")
        return tr("BETRIEB", "OPERATION")
    }

    function pageTitle() {
        if (page === 1)
            return tr("Kalibrierung", "Calibration")
        if (page === 3)
            return tr("Alarm", "Alarm")
        return tr("Betriebsübersicht", "Operation Overview")
    }

    function refreshTexts() {
        faultText = faultDisplay()
        if (ackKey === "ok")
            ackStatus = tr("quittiert", "acknowledged")
        else if (ackKey === "locked")
            ackStatus = tr("gesperrt", "locked")
        else
            ackStatus = ""
        if (severity <= 0)
            statusLine = tr("Prozess im Sollbereich", "Process within range")
        else if (alarmAcked)
            statusLine = tr("Quittiert. Zustand bleibt bis der Prozess gesund ist.",
                            "Acknowledged. Condition remains until the process is healthy.")
        else
            statusLine = faultText
    }

    function recompute() {
        var nextSev = 0
        var nextKey = ""
        function consider(level, key) {
            if (level > nextSev) {
                nextSev = level
                nextKey = key
            }
        }
        if (tempC < 0.0 || tempC > 40.0)
            consider(2, "temp-alarm")
        else if (tempC < 5.0 || tempC > 32.0)
            consider(1, "temp-warn")
        if (humPct < 15 || humPct > 90)
            consider(2, "hum-alarm")
        else if (humPct < 25 || humPct > 75)
            consider(1, "hum-warn")
        if (distanceCm >= 0) {
            if (distanceCm < 20)
                consider(2, "dist-alarm")
            else if (distanceCm < 50)
                consider(1, "dist-warn")
        }
        severity = nextSev
        faultKey = nextKey
        zone = nextSev === 0 ? "SAFE" : (nextSev === 1 ? "WARN" : "FAULT")
        if (nextSev === 0)
            alarmAcked = false
        refreshTexts()
    }

    function acknowledge() {
        if (!(ackArmed || !linkNfc)) {
            ackFailed = true
            ackKey = "locked"
            ackOpen = true
            refreshTexts()
            return
        }
        alarmAcked = true
        ackFailed = false
        ackKey = "ok"
        ackOpen = true
        refreshTexts()
        var copy = events.slice()
        copy.unshift({
            t: Qt.formatTime(new Date(), "hh:mm"),
            text: tr("Quittiert", "Acknowledged")
        })
        events = copy
    }

    function pushEvent(text) {
        var copy = events.slice()
        copy.unshift({ t: Qt.formatTime(new Date(), "hh:mm"), text: text })
        events = copy
    }

    onTempCChanged: recompute()
    onHumPctChanged: recompute()
    onDistanceCmChanged: recompute()
    onLangEnChanged: refreshTexts()
    onAlarmAckedChanged: refreshTexts()

    Component.onCompleted: recompute()

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: vm.clockText = Qt.formatTime(new Date(), "hh:mm:ss")
    }
}
