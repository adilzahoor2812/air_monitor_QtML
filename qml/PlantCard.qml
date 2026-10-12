import QtQuick
import QtQuick.Layouts
import AirMonitor

Card {
    id: root

    property var vm

    title: (vm.langEn ? "PLANT" : "ANLAGE")

    readonly property bool ackEnabled: vm.ackArmed || !vm.linkNfc

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 8
            color: Theme.severityColor(vm.severity)

            Text {
                anchors.centerIn: parent
                width: parent.width - 4
                text: vm.stateWord
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font28
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: (vm.langEn ? "Fault:" : "Störung:")
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }

            Text {
                text: vm.faultDisplay
                color: Theme.value
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                Layout.fillWidth: true
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Theme.ackFill
            border.color: Theme.ackBorder
            border.width: 1
            radius: 0
            opacity: root.ackEnabled ? 1 : 0.45

            Text {
                anchors.centerIn: parent
                width: parent.width - 4
                text: (vm.langEn ? "Acknowledge" : "Quittieren")
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.ackEnabled
                onClicked: vm.acknowledge()
            }
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            text: vm.ackArmed
                  ? (vm.langEn ? "released %1s" : "freigegeben %1s").arg(vm.releaseSeconds)
                  : (vm.langEn ? "Operator Release" : "Operator-Freigabe")
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font12
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }

        Text {
            Layout.fillWidth: true
            text: vm.ackStatus
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font12
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
    }
}
