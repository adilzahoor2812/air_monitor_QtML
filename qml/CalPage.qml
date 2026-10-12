import QtQuick
import QtQuick.Layouts
import AirMonitor

Card {
    id: root

    property var vm

    title: (vm.langEn ? "CALIBRATION" : "KALIBRIERUNG")

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            width: parent.width
            text: (vm.langEn ? "Temp warn 5.0 \u2026 32.0 \u00B0C" : "Temp warn 5.0 \u2026 32.0 \u00B0C")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        Text {
            width: parent.width
            text: (vm.langEn ? "Temp alarm 0.0 \u2026 40.0 \u00B0C" : "Temp alarm 0.0 \u2026 40.0 \u00B0C")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        Text {
            width: parent.width
            text: (vm.langEn ? "Humidity warn 25 \u2026 75 %" : "Feuchte warn 25 \u2026 75 %")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        Text {
            width: parent.width
            text: (vm.langEn ? "Humidity alarm 15 \u2026 90 %" : "Feuchte alarm 15 \u2026 90 %")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        Text {
            width: parent.width
            text: (vm.langEn ? "Distance warn < 50 cm" : "Distanz warn < 50 cm")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        Text {
            width: parent.width
            text: (vm.langEn ? "Distance alarm < 20 cm" : "Distanz alarm < 20 cm")
            color: Theme.title
            font.family: Theme.fontFamily
            font.pixelSize: Theme.font14
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
    }
}
