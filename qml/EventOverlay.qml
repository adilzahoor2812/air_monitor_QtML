import QtQuick
import QtQuick.Layouts
import AirMonitor

Item {
    id: root

    property var vm

    visible: vm && vm.logOpen

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.35

        MouseArea {
            anchors.fill: parent
            onClicked: vm.logOpen = false
        }
    }

    Rectangle {
        width: 420
        height: 260
        anchors.centerIn: parent
        radius: 0
        color: Theme.panel
        border.color: Theme.line
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
                    text: (vm.langEn ? "EVENT LOG" : "PROTOKOLL")
                    color: "white"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font12
                    font.letterSpacing: 1
                    font.capitalization: Font.AllUppercase
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    visible: vm.events.length === 0
                    text: (vm.langEn ? "no events" : "keine Ereignisse")
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font14
                    wrapMode: Text.NoWrap
                }

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    visible: vm.events.length > 0
                    clip: true
                    spacing: 0
                    model: vm.events
                    delegate: Text {
                        required property var modelData
                        width: ListView.view.width
                        height: 18
                        text: modelData.t + "  " + modelData.text
                        color: Theme.title
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.font12
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                Layout.margins: 8
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
                    onClicked: vm.logOpen = false
                }
            }
        }
    }
}
