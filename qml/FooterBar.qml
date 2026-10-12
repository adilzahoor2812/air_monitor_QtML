import QtQuick
import QtQuick.Layouts
import AirMonitor

Rectangle {
    id: root

    property var vm

    color: Theme.footer
    radius: 0

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.fkeyRule
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: [
                { page: 0, de: "Menü", en: "Menu" },
                { page: 1, de: "Kalibrierung", en: "Calibration" },
                { page: -1, de: "Protokoll", en: "Log" },
                { page: 3, de: "Alarm", en: "Alarm" }
            ]

            Rectangle {
                id: key
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 0
                color: (modelData.page >= 0 && vm.page === modelData.page) ? Theme.fkeyOn : Theme.footer

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 4
                    text: vm.langEn ? modelData.en : modelData.de
                    color: (modelData.page >= 0 && vm.page === modelData.page) ? Theme.header : Theme.title
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font12
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: index < 3 ? 1 : 0
                    color: Theme.fkeyRule
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (modelData.page < 0) {
                            vm.logOpen = true
                        } else {
                            vm.page = modelData.page
                            vm.logOpen = false
                        }
                    }
                }
            }
        }
    }
}
