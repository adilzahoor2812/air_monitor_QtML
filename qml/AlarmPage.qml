import QtQuick
import QtQuick.Layouts
import AirMonitor

Card {
    id: root

    property var vm

    title: (vm.langEn ? "ALARM" : "ALARM")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: vm.severity >= 2 ? (vm.langEn ? "FAULT" : "STÖRUNG")
                  : (vm.severity === 1 ? (vm.langEn ? "WARNING" : "WARNUNG")
                                       : (vm.langEn ? "READY" : "BEREIT"))
            color: Theme.severityColor(vm.severity)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font28
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            text: vm.severity <= 0
                  ? (vm.langEn ? "No active fault. All measurements are within range." : "Keine aktive Störung. Alle Messwerte liegen im Sollbereich.")
                  : (vm.langEn ? "Active condition: %1. It remains until the process is healthy." : "Aktive Bedingung: %1. Sie bleibt bestehen, bis der Prozess wieder gesund ist.").arg(vm.faultDisplay)
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: vm.zone
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            wrapMode: Text.NoWrap
        }

        Text {
            text: vm.statusLine
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font12
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
