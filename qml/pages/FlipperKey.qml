import QtQuick 2.6
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0

// Round remote key. The visual is a translucent circle (tone like the d-pad
// arrows, never solid black) rendered together with a soft, diffuse drop
// shadow that darkens and drops further while pressed. No visible border.
Item {
    id: root
    property alias text: label.text
    property bool accent: false
    property bool circular: true
    property real diameter: Theme.itemSizeLarge
    signal clicked()

    width: diameter
    height: diameter

    // The button look (also the shadow's source, so it stays translucent).
    Item {
        id: content
        anchors.fill: parent
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: root.circular ? width / 2 : Theme.paddingMedium
            color: root.accent ? (mouse.pressed ? "#ffa640" : "#FF8200")
                               : (mouse.pressed ? Qt.rgba(0, 0, 0, 0.30) : Qt.rgba(0, 0, 0, 0.18))
        }
        Label {
            anchors.centerIn: parent
            text: label.text
            font.bold: true
            font.pixelSize: diameter * 0.5
            color: root.accent ? "#201200" : Qt.rgba(0, 0, 0, 0.6)
        }
    }
    DropShadow {
        anchors.fill: content
        source: content
        horizontalOffset: 0
        verticalOffset: mouse.pressed ? diameter * 0.16 : diameter * 0.10
        radius: mouse.pressed ? diameter * 0.32 : diameter * 0.24
        samples: 25
        color: mouse.pressed ? Qt.rgba(0, 0, 0, 0.72) : Qt.rgba(0, 0, 0, 0.4)
        Behavior on color { ColorAnimation { duration: 90 } }
    }

    // hidden helper so `text` alias has a target
    Label { id: label; visible: false }

    MouseArea { id: mouse; anchors.fill: parent; onClicked: root.clicked() }
}
