import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AirMonitor

ApplicationWindow {
    id: window

    width: 480
    height: 320
    minimumWidth: 480
    maximumWidth: 480
    minimumHeight: 320
    maximumHeight: 320
    title: "AIR MONITOR"
    visible: true
    color: Theme.bg
    font.family: Theme.fontFamily
    font.pixelSize: Theme.font12

    Component.onCompleted: {
        if (Screen.width > width && Screen.height > height) {
            x = Math.round((Screen.width - width) / 2)
            y = Math.round((Screen.height - height) / 2)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            vm: monitor
            Layout.fillWidth: true
            Layout.preferredHeight: 28
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: monitor.liveMode && !monitor.linkEth ? 18 : 0
            visible: monitor.liveMode && !monitor.linkEth
            color: "#F5D0C8"
            Text {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
                text: {
                    var base = monitor.langEn
                            ? ("LIVE offline — set Jetson Ethernet to 192.168.10.10/24, then reach "
                               + monitor.host + ":" + monitor.port)
                            : ("LIVE offline — Jetson-Ethernet auf 192.168.10.10/24 setzen, dann "
                               + monitor.host + ":" + monitor.port)
                    return base + "  [" + monitor.connectionText + "]"
                }
                color: Theme.alarm
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            HomePage {
                anchors.fill: parent
                anchors.margins: 6
                visible: monitor.page === 0
                vm: monitor
            }

            CalPage {
                anchors.fill: parent
                anchors.margins: 6
                visible: monitor.page === 1
                vm: monitor
            }

            AlarmPage {
                anchors.fill: parent
                anchors.margins: 6
                visible: monitor.page === 3
                vm: monitor
            }
        }

        FooterBar {
            vm: monitor
            Layout.fillWidth: true
            Layout.preferredHeight: 36
        }
    }

    EventOverlay {
        anchors.fill: parent
        z: 10
        vm: monitor
    }

    AckDialog {
        anchors.fill: parent
        z: 20
        vm: monitor
    }
}
