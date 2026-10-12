import QtQuick
import QtQuick.Layouts
import AirMonitor

Item {
    id: root

    property var vm

    visible: vm && vm.ackOpen

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.35
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        width: 360
        height: 168
        anchors.centerIn: parent
        radius: 0
        color: Theme.panel
        border.color: Theme.ackBorder
        border.width: 1

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: Theme.header
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    text: vm.ackFailed
                          ? (vm.langEn ? "ACKNOWLEDGE ERROR" : "QUITTIERUNG FEHLER")
                          : (vm.langEn ? "ACKNOWLEDGE" : "QUITTIERUNG")
                    color: "white"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font12
                    font.letterSpacing: 1
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 12
                text: vm.ackFailed
                      ? (vm.langEn ? "Acknowledge is locked until operator release is active. Operator: OP-01" : "Quittieren ist gesperrt, bis die Operator-Freigabe aktiv ist. Operator: OP-01")
                      : (vm.langEn ? "The condition remains until the process is healthy. Operator: OP-01" : "Die Bedingung bleibt bestehen, bis der Prozess wieder gesund ist. Operator: OP-01")
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font14
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 12
                Layout.bottomMargin: 10
                Layout.preferredWidth: 96
                Layout.preferredHeight: 32
                color: Theme.ackFill
                border.color: Theme.ackBorder
                border.width: 1
                radius: 0
                Text {
                    anchors.centerIn: parent
                    text: (vm.langEn ? "Close" : "Schließen")
                    color: Theme.title
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font12
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: vm.ackOpen = false
                }
            }
        }
    }
}
