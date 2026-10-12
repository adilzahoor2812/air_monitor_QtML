import QtQuick
import QtQuick.Layouts
import AirMonitor

Card {
    id: root

    property var vm

    title: (vm.langEn ? "EVENTS" : "EREIGNISSE")

    Column {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 2
        spacing: 0

        Repeater {
            model: Math.min(4, vm.events.length)

            Text {
                required property int index
                width: parent.width
                height: 16
                text: vm.events[index].t + "  " + vm.events[index].text
                color: Theme.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.font12
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
