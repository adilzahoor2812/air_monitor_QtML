import QtQuick
import QtQuick.Layouts
import AirMonitor

Rectangle {
    id: root

    property string title: ""
    default property alias contents: body.data

    color: Theme.panel
    border.color: Theme.line
    border.width: 1
    radius: 0
    clip: true

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
                text: root.title
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
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }
    }
}
