pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#E8E8E8"
    readonly property color panel: "#FFFFFF"
    readonly property color header: "#1A4A8C"
    readonly property color footer: "#D8D8D8"
    readonly property color line: "#C8C8C8"
    readonly property color title: "#111111"
    readonly property color muted: "#555555"
    readonly property color value: "#000000"
    readonly property color bar: "#2F6DB3"
    readonly property color safe: "#3D9B47"
    readonly property color warn: "#D48912"
    readonly property color alarm: "#C42B28"
    readonly property color blackHdr: "#000000"
    readonly property color langIdle: "#2A2A2A"
    readonly property color fkeyOn: "#C5D4E8"
    readonly property color ackFill: "#E4E4E4"
    readonly property color ackBorder: "#222222"
    readonly property color barTrack: "#D0D0D0"
    readonly property color fkeyRule: "#B0B0B0"
    readonly property color lampOff: "#555555"
    readonly property int font12: 12
    readonly property int font14: 14
    readonly property int font16: 16
    readonly property int font20: 20
    readonly property int font28: 28

    readonly property string fontFamily: {
        var families = Qt.fontFamilies()
        if (families.indexOf("Roboto") >= 0)
            return "Roboto"
        if (families.indexOf("Noto Sans") >= 0)
            return "Noto Sans"
        return "DejaVu Sans"
    }

    function severityColor(severity) {
        if (severity >= 2)
            return alarm
        if (severity === 1)
            return warn
        return safe
    }
}
