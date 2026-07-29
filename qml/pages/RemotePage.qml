import QtQuick 2.6
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    // Flipper input key codes (PB_Gui_InputKey).
    readonly property int keyUp: 0
    readonly property int keyDown: 1
    readonly property int keyRight: 2
    readonly property int keyLeft: 3
    readonly property int keyOk: 4
    readonly property int keyBack: 5

    readonly property bool landscape: width > height
    readonly property real bezelPad: Theme.paddingMedium
    readonly property real leftMargin: Theme.horizontalPageMargin * 1.6

    // Flipper screen is 128x64 → 2:1.
    readonly property real imgW: {
        if (landscape) {
            var byW = width * 0.48 - 2 * bezelPad
            var byH = (height - 2 * Theme.paddingLarge - 2 * bezelPad) * 2
            return Math.min(byW, byH)
        }
        return width - 2 * Theme.horizontalPageMargin - 2 * bezelPad
    }
    readonly property real imgH: imgW / 2
    readonly property real sw: imgW + 2 * bezelPad
    readonly property real sh: imgH + 2 * bezelPad

    readonly property real dpadDia: landscape
        ? Math.min(width * 0.34, imgH + 2 * bezelPad)
        : Math.min(width * 0.66, height * 0.40)
    readonly property real backDia: dpadDia * 0.24

    // Positions — screen window and d-pad share the same vertical centre.
    readonly property real screenX: landscape ? leftMargin : (width - sw) / 2
    readonly property real screenY: landscape ? (height - sh) / 2
                                             : infoPill.y + infoPill.height + Theme.paddingLarge * 2
    readonly property real regionStart: screenX + sw + Theme.paddingLarge
    readonly property real dpadX: landscape
        ? regionStart + Math.max(0, (width - regionStart - Theme.horizontalPageMargin - dpadDia) / 2)
        : (width - dpadDia) / 2
    readonly property real dpadY: landscape ? (height - dpadDia) / 2
                                           : screenY + sh + Theme.itemSizeMedium
    // Back button centre: down-right of the disc.
    readonly property real backCx: landscape ? dpadX + dpadDia + backDia * 0.15
                                             : dpadX + dpadDia + backDia * 0.15
    readonly property real backCy: landscape ? dpadY + dpadDia + backDia * 0.05
                                             : dpadY + dpadDia + backDia * 0.7

    Component.onCompleted: if (flipper) flipper.startScreen()
    Component.onDestruction: if (flipper) flipper.stopScreen()

    // --- Orange full-bleed background ---------------------------------------
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#FF9E2C" }
            GradientStop { position: 1.0; color: "#F07300" }
        }
    }

    // --- Info bar: name · connection · battery ------------------------------
    Rectangle {
        id: infoPill
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: Theme.paddingMedium
        }
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.28)
        height: Theme.itemSizeExtraSmall
        width: infoRow.width + 2 * Theme.paddingLarge

        Row {
            id: infoRow
            anchors.centerIn: parent
            spacing: Theme.paddingMedium

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: flipper.deviceName.length ? flipper.deviceName : "Flipper"
                color: "white"; font.bold: true
                font.pixelSize: Theme.fontSizeSmall
            }
            Rectangle { width: 2; height: Theme.fontSizeSmall; color: Qt.rgba(1,1,1,0.4)
                        anchors.verticalCenter: parent.verticalCenter }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: flipper.transportName.length ? flipper.transportName : qsTr("—")
                color: Qt.rgba(1,1,1,0.85); font.pixelSize: Theme.fontSizeSmall
            }
            Rectangle { width: 2; height: Theme.fontSizeSmall; color: Qt.rgba(1,1,1,0.4)
                        anchors.verticalCenter: parent.verticalCenter }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall / 2
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.fontSizeLarge; height: Theme.fontSizeSmall
                    radius: 2; color: "transparent"
                    border.width: 1; border.color: "white"
                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 2 }
                        width: Math.max(0, (parent.width - 4) * Math.max(0, flipper.batteryPercent) / 100)
                        radius: 1
                        color: flipper.batteryPercent < 15 ? "#ff5252"
                             : flipper.batteryPercent < 40 ? "#ffd24d" : "#7CFC7C"
                        visible: flipper.batteryPercent >= 0
                    }
                }
                Rectangle { anchors.verticalCenter: parent.verticalCenter
                            width: 2; height: Theme.fontSizeExtraSmall; color: "white" }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: flipper.batteryPercent >= 0 ? flipper.batteryPercent + "%" : "–"
                    color: "white"; font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }

    // --- Screen window (dark bezel + drop shadow + live screen) --------------
    Item {
        id: screenWindow
        width: page.sw
        height: page.sh
        x: page.screenX
        y: page.screenY

        Rectangle {
            id: bezel
            anchors.fill: parent
            radius: Theme.paddingMedium
            color: "#111214"
            visible: false
        }
        DropShadow {
            anchors.fill: bezel
            source: bezel
            horizontalOffset: 0
            verticalOffset: 8
            radius: 26
            samples: 33
            color: Qt.rgba(0, 0, 0, 0.5)
        }
        Image {
            anchors.centerIn: parent
            width: page.imgW
            height: page.imgH
            smooth: false
            cache: false
            fillMode: Image.Stretch
            source: "image://flipperscreen/f" + flipper.screenRevision
        }

        // Touch the mirrored screen. The app only has pixels (no menu model),
        // so a tap can't jump to a specific line — instead:
        //   swipe          → direction key (up/down/left/right)
        //   tap top / bottom → ▲ / ▼  (move the selection bar into position)
        //   tap middle       → OK    (activate the highlighted entry)
        MouseArea {
            anchors.centerIn: parent
            width: page.imgW
            height: page.imgH
            property real sx: 0
            property real sy: 0
            onPressed: { sx = mouseX; sy = mouseY }
            onReleased: {
                var dx = mouseX - sx
                var dy = mouseY - sy
                var thr = Math.max(Theme.paddingLarge, width * 0.10)
                if (Math.abs(dx) < thr && Math.abs(dy) < thr) {
                    var ry = mouseY / height
                    if (ry < 0.38)
                        flipper.pressKey(page.keyUp)
                    else if (ry > 0.62)
                        flipper.pressKey(page.keyDown)
                    else
                        flipper.pressKey(page.keyOk)
                } else if (Math.abs(dy) >= Math.abs(dx)) {
                    flipper.pressKey(dy < 0 ? page.keyUp : page.keyDown)
                } else {
                    flipper.pressKey(dx < 0 ? page.keyLeft : page.keyRight)
                }
            }
        }
    }

    // --- Round d-pad disc ---------------------------------------------------
    DPad {
        id: dpadCtl
        x: page.dpadX
        y: page.dpadY
        diameter: page.dpadDia
        onKeyPressed: flipper.pressKey(key)
    }

    // --- Round back button (down-right of the disc, no border, soft shadow) --
    FlipperKey {
        id: backBtn
        diameter: page.backDia
        text: "↩"
        x: page.backCx - width / 2
        y: page.backCy - height / 2
        onClicked: flipper.pressKey(page.keyBack)
    }
}
