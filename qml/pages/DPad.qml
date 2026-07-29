import QtQuick 2.6
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0

// Circular d-pad: hollow outline disc (no shadow), four arrow keys (each with a
// soft diffuse shadow) and a central OK button with a soft diffuse shadow.
// Nothing is solid black — sources are translucent so they read like glyphs.
Item {
    id: dpad
    property real diameter: Theme.itemSizeHuge
    signal keyPressed(int key)          // PB_Gui_InputKey: Up0 Down1 Right2 Left3 Ok4

    width: diameter
    height: diameter

    readonly property color glyph: Qt.rgba(0, 0, 0, 0.6)
    readonly property color glyphDown: Qt.rgba(0, 0, 0, 0.85)
    readonly property real hit: diameter * 0.30
    readonly property real okSize: diameter * 0.34
    readonly property real arrowPx: diameter * 0.15
    readonly property real arrowShadowOff: diameter * 0.035
    readonly property real arrowShadowRad: diameter * 0.06

    // outer disc — outline only, no fill, no shadow
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: Math.max(2, diameter * 0.013)
        border.color: Qt.rgba(0, 0, 0, 0.32)
    }

    // --- directional arrows (glyph rendered with a diffuse drop shadow) ---
    MouseArea {
        id: upA
        width: dpad.hit; height: dpad.hit
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: diameter * 0.05 }
        onClicked: dpad.keyPressed(0)
        Label { id: upG; anchors.centerIn: parent; text: "▲"; font.pixelSize: dpad.arrowPx
                color: upA.pressed ? dpad.glyphDown : dpad.glyph; visible: false }
        DropShadow { anchors.fill: upG; source: upG; horizontalOffset: 0
                     verticalOffset: dpad.arrowShadowOff; radius: dpad.arrowShadowRad
                     samples: 25; color: Qt.rgba(0, 0, 0, 0.33) }
    }
    MouseArea {
        id: downA
        width: dpad.hit; height: dpad.hit
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: diameter * 0.05 }
        onClicked: dpad.keyPressed(1)
        Label { id: downG; anchors.centerIn: parent; text: "▼"; font.pixelSize: dpad.arrowPx
                color: downA.pressed ? dpad.glyphDown : dpad.glyph; visible: false }
        DropShadow { anchors.fill: downG; source: downG; horizontalOffset: 0
                     verticalOffset: dpad.arrowShadowOff; radius: dpad.arrowShadowRad
                     samples: 25; color: Qt.rgba(0, 0, 0, 0.33) }
    }
    MouseArea {
        id: leftA
        width: dpad.hit; height: dpad.hit
        anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: diameter * 0.05 }
        onClicked: dpad.keyPressed(3)
        Label { id: leftG; anchors.centerIn: parent; text: "◀"; font.pixelSize: dpad.arrowPx
                color: leftA.pressed ? dpad.glyphDown : dpad.glyph; visible: false }
        DropShadow { anchors.fill: leftG; source: leftG; horizontalOffset: 0
                     verticalOffset: dpad.arrowShadowOff; radius: dpad.arrowShadowRad
                     samples: 25; color: Qt.rgba(0, 0, 0, 0.33) }
    }
    MouseArea {
        id: rightA
        width: dpad.hit; height: dpad.hit
        anchors { verticalCenter: parent.verticalCenter; right: parent.right; rightMargin: diameter * 0.05 }
        onClicked: dpad.keyPressed(2)
        Label { id: rightG; anchors.centerIn: parent; text: "▶"; font.pixelSize: dpad.arrowPx
                color: rightA.pressed ? dpad.glyphDown : dpad.glyph; visible: false }
        DropShadow { anchors.fill: rightG; source: rightG; horizontalOffset: 0
                     verticalOffset: dpad.arrowShadowOff; radius: dpad.arrowShadowRad
                     samples: 25; color: Qt.rgba(0, 0, 0, 0.33) }
    }

    // --- centre OK (translucent source + soft diffuse shadow) ---
    Item {
        id: okContent
        anchors.centerIn: parent
        width: dpad.okSize; height: dpad.okSize
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: okArea.pressed ? Qt.rgba(0, 0, 0, 0.34) : Qt.rgba(0, 0, 0, 0.22)
        }
        Label {
            anchors.centerIn: parent
            text: "OK"; font.bold: true
            font.pixelSize: diameter * 0.11
            color: Qt.rgba(1, 1, 1, 0.92)
        }
    }
    DropShadow {
        anchors.fill: okContent
        source: okContent
        horizontalOffset: 0
        verticalOffset: okArea.pressed ? diameter * 0.05 : diameter * 0.032
        radius: okArea.pressed ? diameter * 0.11 : diameter * 0.08
        samples: 25
        color: okArea.pressed ? Qt.rgba(0, 0, 0, 0.72) : Qt.rgba(0, 0, 0, 0.42)
        Behavior on color { ColorAnimation { duration: 90 } }
    }
    MouseArea {
        id: okArea
        anchors.centerIn: parent
        width: dpad.okSize; height: dpad.okSize
        onClicked: dpad.keyPressed(4)
    }
}
