import QtQuick
import QtQuick.Layouts
import AirMonitor

RowLayout {
    id: root

    property var vm

    spacing: 6

    function clamp01(value) {
        return Math.max(0, Math.min(1, value))
    }

    PlantCard {
        vm: root.vm
        Layout.preferredWidth: 128
        Layout.minimumWidth: 128
        Layout.maximumWidth: 128
        Layout.fillHeight: true
    }

    Card {
        title: (vm.langEn ? "MEASURED VALUES" : "MESSWERTE")
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            MetricBlock {
                name: (vm.langEn ? "Distance" : "Distanz")
                valueText: (!vm.hasSample || vm.distanceCm < 0) ? "\u2014" : (vm.distanceCm + " ")
                unit: "cm"
                // HC-SR04 scale 0..400 cm
                fraction: (!vm.hasSample || vm.distanceCm < 0) ? 0 : root.clamp01(vm.distanceCm / 400.0)
                ticks: [
                    { t: "0", p: 0 },
                    { t: "100", p: 0.25 },
                    { t: "200", p: 0.5 },
                    { t: "400", p: 1 }
                ]
                showSpark: true
                sparkPoints: vm.distSpark
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            MetricBlock {
                name: (vm.langEn ? "Temperature" : "Temperatur")
                valueText: (vm.liveMode && !vm.hasSample) ? "\u2014" : vm.tempC.toFixed(1)
                unit: "\u00B0C"
                fraction: root.clamp01((vm.tempC + 20) / 100)
                ticks: [
                    { t: "-20", p: 0 },
                    { t: "0", p: 0.2 },
                    { t: "50", p: 0.7 },
                    { t: "80", p: 1 }
                ]
                Layout.fillWidth: true
            }

            MetricBlock {
                name: (vm.langEn ? "Humidity" : "Feuchte")
                valueText: (vm.liveMode && !vm.hasSample) ? "\u2014" : vm.humPct.toFixed(1)
                unit: "%"
                fraction: root.clamp01(vm.humPct / 100)
                ticks: [{ t: "0", p: 0 }, { t: "50", p: 0.5 }, { t: "100", p: 1 }]
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        Layout.preferredWidth: 132
        Layout.minimumWidth: 132
        Layout.maximumWidth: 132
        Layout.fillHeight: true
        spacing: 6

        SystemCard {
            vm: root.vm
            Layout.fillWidth: true
            Layout.preferredHeight: 152
        }

        EventsCard {
            vm: root.vm
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
