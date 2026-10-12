import QtQuick
import QtQuick.Layouts
import AirMonitor

ColumnLayout {
    id: root

    property string name: ""
    property string valueText: ""
    property string unit: ""
    property real fraction: 0
    property var ticks: []
    property bool showSpark: false
    property var sparkPoints: []

    spacing: 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: root.showSpark
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.topMargin: 3
        Layout.bottomMargin: 3
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: root.name
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font14
                Layout.preferredWidth: 86
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }

            Text {
                text: root.valueText
                color: Theme.value
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font20
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }

            Text {
                text: root.unit
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 28
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            radius: 0
            color: Theme.barTrack

            Rectangle {
                width: Math.max(0, Math.min(parent.width, parent.width * Math.max(0, Math.min(1, root.fraction))))
                height: parent.height
                radius: 2
                color: Theme.bar
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 12

            Repeater {
                model: root.ticks
                delegate: Text {
                    required property var modelData
                    text: modelData.t
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.font12
                    wrapMode: Text.NoWrap
                    x: {
                        var slot = modelData.p * (parent.width - width)
                        return Math.max(0, Math.min(parent.width - width, slot))
                    }
                }
            }
        }

        Sparkline {
            visible: root.showSpark
            points: root.sparkPoints
            Layout.fillWidth: true
            Layout.fillHeight: root.showSpark
            Layout.preferredHeight: root.showSpark ? 24 : 0
            Layout.maximumHeight: root.showSpark ? 4096 : 0
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.line
    }
}
