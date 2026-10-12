import QtQuick
import QtQuick.Layouts
import AirMonitor

Rectangle {
    id: root

    property var vm

    color: Theme.blackHdr
    radius: 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 6

        Text {
            text: "AIR MONITOR"
            color: "white"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font12
            Layout.preferredWidth: 92
            Layout.maximumWidth: 92
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }

        Rectangle {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 18
            Layout.alignment: Qt.AlignVCenter
            radius: 2
            color: Theme.langIdle

            Row {
                anchors.centerIn: parent
                spacing: 0

                Rectangle {
                    width: 24
                    height: 18
                    radius: 2
                    color: vm.langEn ? "transparent" : Theme.header
                    Text {
                        anchors.centerIn: parent
                        text: "DE"
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.font12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: vm.langEn = false
                    }
                }

                Rectangle {
                    width: 24
                    height: 18
                    radius: 2
                    color: vm.langEn ? Theme.header : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "EN"
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.font12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: vm.langEn = true
                    }
                }
            }
        }

        Text {
            text: vm.pageTitle
            color: "white"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font12
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }

        RowLayout {
            Layout.preferredWidth: 168
            Layout.maximumWidth: 168
            spacing: 4

            Rectangle {
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
                Layout.alignment: Qt.AlignVCenter
                radius: 6
                color: {
                    if (vm.liveMode && !vm.linkEth)
                        return Theme.alarm
                    if (vm.severity > 0 && vm.alarmAcked)
                        return Theme.lampOff
                    return Theme.severityColor(vm.severity)
                }
            }

            Text {
                text: vm.headerWord
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                Layout.fillWidth: true
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }

            Text {
                text: vm.clockText
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
        }
    }
}
