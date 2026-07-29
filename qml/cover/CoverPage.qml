import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "file:///usr/share/icons/hicolor/172x172/apps/harbour-sflipper.png"
            width: parent.width * 0.55
            height: width
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: flipper.connected ? (flipper.deviceName.length ? flipper.deviceName : "sFlipper")
                                    : "sFlipper"
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.highlightColor
            truncationMode: TruncationMode.Fade
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: flipper.connected
                  ? (flipper.batteryPercent >= 0 ? flipper.batteryPercent + " %" : qsTr("connected"))
                  : qsTr("offline")
            font.pixelSize: Theme.fontSizeSmall
            color: flipper.connected ? "#FF8200" : Theme.secondaryColor
        }
    }

    CoverActionList {
        enabled: flipper.connected
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: flipper.refreshInfo()
        }
    }
}
