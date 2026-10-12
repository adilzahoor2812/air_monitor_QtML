import QtQuick
import QtQuick.Layouts
import AirMonitor

Card {
    id: root

    property var vm

    title: (vm.langEn ? "SYSTEM" : "SYSTEM")

    Column {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 0

        RowLayout {
            width: parent.width
            height: 16
            Text {
                text: "Ethernet"
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            Item { Layout.fillWidth: true }
            Text {
                text: vm.linkEth ? (vm.langEn ? "Connected" : "Verbunden") : (vm.langEn ? "Offline" : "Getrennt")
                color: vm.linkEth ? Theme.safe : Theme.alarm
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.line }

        RowLayout {
            width: parent.width
            height: 16
            Text {
                text: "FreeRTOS"
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (vm.liveMode && !vm.hasSample) ? "\u2014" : "OK"
                color: (vm.liveMode && !vm.hasSample) ? Theme.muted : Theme.safe
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.line }

        RowLayout {
            width: parent.width
            height: 16
            Text {
                text: "Watchdog"
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (vm.liveMode && !vm.hasSample) ? "\u2014" : "OK"
                color: (vm.liveMode && !vm.hasSample) ? Theme.muted : Theme.safe
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.line }

        RowLayout {
            width: parent.width
            height: 16
            Text {
                text: "CPU"
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (vm.liveMode && !vm.hasSample) ? "\u2014" : (vm.langEn ? "%1 %" : "%1 %").arg(vm.cpuPct)
                color: Theme.value
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.line }

        RowLayout {
            width: parent.width
            height: 16
            Text {
                text: "Heap"
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                wrapMode: Text.NoWrap
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (vm.liveMode && !vm.hasSample)
                      ? "\u2014"
                      : (vm.langEn ? "%1 % free" : "%1 % frei").arg(vm.heapFreePct)
                color: Theme.value
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.line }
    }
}
