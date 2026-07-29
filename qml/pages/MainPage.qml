import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    // Flipper brand accent, used sparingly over the native Silica look.
    readonly property color flipperOrange: "#FF8200"

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Disconnect")
                visible: flipper.connected
                onClicked: flipper.disconnectDevice()
            }
            MenuItem {
                text: qsTr("Refresh")
                visible: flipper.connected
                onClicked: flipper.refreshInfo()
            }
            MenuItem {
                text: qsTr("Connect via BLE")
                visible: !flipper.connected
                onClicked: flipper.connectBle()
            }
            MenuItem {
                text: qsTr("Connect via USB-C")
                visible: !flipper.connected
                onClicked: flipper.connectUsb()
            }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("sFlipper") }

            // --- Connection status ---------------------------------------
            Item {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: statusRow.height

                Row {
                    id: statusRow
                    spacing: Theme.paddingMedium

                    Rectangle {
                        width: Theme.fontSizeMedium
                        height: width
                        radius: width / 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: flipper.connected ? page.flipperOrange : Theme.secondaryColor
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: flipper.statusText
                        color: flipper.connected ? Theme.highlightColor : Theme.secondaryHighlightColor
                    }
                }
            }

            // --- Connected: device summary + navigation ------------------
            Column {
                width: parent.width
                spacing: Theme.paddingSmall
                visible: flipper.connected

                DetailItem {
                    label: qsTr("Name")
                    value: flipper.deviceName
                }
                DetailItem {
                    label: qsTr("Model")
                    value: flipper.hardwareModel
                }
                DetailItem {
                    label: qsTr("Firmware")
                    value: flipper.firmwareVersion
                }
                DetailItem {
                    label: qsTr("Battery")
                    value: flipper.batteryPercent >= 0 ? flipper.batteryPercent + " %" : "–"
                }

                Item { width: 1; height: Theme.paddingMedium }

                BackgroundItem {
                    width: parent.width
                    onClicked: pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"))
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Device info (all fields)")
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
                BackgroundItem {
                    width: parent.width
                    onClicked: pageStack.push(Qt.resolvedUrl("RemotePage.qml"))
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Screen & remote control")
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
                BackgroundItem {
                    width: parent.width
                    onClicked: pageStack.push(Qt.resolvedUrl("FilesPage.qml"))
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Files & saved signals")
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
                BackgroundItem {
                    width: parent.width
                    onClicked: pageStack.push(Qt.resolvedUrl("FirmwarePage.qml"))
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Firmware update")
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
            }

            // --- Not connected: hint -------------------------------------
            Label {
                visible: !flipper.connected
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Connect the Flipper via USB-C (or BLE, coming soon), "
                         + "then pull down to connect.")
            }
        }
    }

    Connections {
        target: flipper
        onActionError: {
            // Lightweight surfacing; a Notification banner can replace this later.
            console.log("Flipper error:", message)
        }
    }
}
