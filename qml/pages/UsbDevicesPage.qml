import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    readonly property color flipperOrange: "#FF8200"

    // Map a canonical class key to a localised section title.
    function classLabel(key) {
        switch (key) {
        case "mouse": return qsTr("Mice")
        case "keyboard": return qsTr("Keyboards")
        case "hid": return qsTr("Other HID")
        case "storage": return qsTr("Mass storage")
        case "audio": return qsTr("Audio")
        case "comm": return qsTr("Serial / CDC")
        case "printer": return qsTr("Printers")
        case "image": return qsTr("Cameras / imaging")
        case "smartcard": return qsTr("Smart cards")
        case "wireless": return qsTr("Wireless")
        case "hub": return qsTr("Hubs")
        default: return qsTr("Other devices")
        }
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: usbLib.devices

        header: Column {
            width: list.width
            PageHeader { title: qsTr("USB devices") }
            Item {
                width: parent.width
                height: flipper.uploadActive ? uploadCol.height + Theme.paddingLarge : 0
                clip: true
                Column {
                    id: uploadCol
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    spacing: Theme.paddingSmall
                    Label {
                        width: parent.width
                        font.pixelSize: Theme.fontSizeSmall
                        color: page.flipperOrange
                        text: flipper.uploadStatus
                    }
                    ProgressBar {
                        width: parent.width
                        value: flipper.uploadProgress
                    }
                }
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Install jiggler app on Flipper")
                visible: flipper.connected && !flipper.uploadActive
                onClicked: flipper.installJigglerFap()
            }
            MenuItem {
                text: qsTr("Read attached device")
                onClicked: pageStack.push(Qt.resolvedUrl("UsbReadPage.qml"))
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No devices saved")
            hintText: qsTr("Plug a USB device (mouse, keyboard, …) into the phone, "
                         + "then pull down and read it.")
        }

        section {
            property: "classKey"
            delegate: SectionHeader {
                text: page.classLabel(section)
            }
        }

        delegate: ListItem {
            id: item
            width: list.width
            contentHeight: Theme.itemSizeMedium

            property var dev: modelData

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Send to Flipper")
                    visible: flipper.connected
                    onClicked: {
                        // Auto-start only over BLE; over USB starting the app
                        // would drop our USB-RPC link, so just write the profile.
                        flipper.pushJigglerProfile(item.dev, flipper.transportName === "BLE")
                    }
                }
                MenuItem {
                    text: qsTr("Rename")
                    onClicked: {
                        var dlg = pageStack.push(Qt.resolvedUrl("RenameUsbDialog.qml"),
                                                 { deviceName: item.dev.name })
                        dlg.accepted.connect(function() {
                            usbLib.renameDevice(item.dev.id, dlg.deviceName)
                        })
                    }
                }
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: item.remorseDelete()
                }
            }

            function remorseDelete() {
                remorseAction(qsTr("Deleting"), function() {
                    usbLib.removeDevice(item.dev.id)
                })
            }

            onClicked: pageStack.push(Qt.resolvedUrl("UsbDeviceDetailPage.qml"),
                                      { deviceId: item.dev.id })

            Column {
                anchors {
                    left: parent.left; leftMargin: Theme.horizontalPageMargin
                    right: parent.right; rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: item.dev.name
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: (item.dev.manufacturer && item.dev.manufacturer.length
                              ? item.dev.manufacturer + " · " : "")
                          + item.dev.vendorId + ":" + item.dev.productId
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
