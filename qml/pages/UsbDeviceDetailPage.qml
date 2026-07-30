import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property string deviceId
    // Re-read from the library so a rename/refresh is reflected live.
    property var dev: usbLib.device(deviceId)

    readonly property color flipperOrange: "#FF8200"
    // Green marks the fields that are actually written to the Flipper profile.
    readonly property color sentColor: "#5FC46A"

    Connections {
        target: usbLib
        onLibraryChanged: page.dev = usbLib.device(page.deviceId)
    }

    // Decode bmAttributes (hex) into a readable power/wakeup summary.
    function attrText(hex) {
        if (!hex) return "–"
        var v = parseInt(hex, 16)
        var parts = []
        parts.push((v & 0x40) ? "self-powered" : "bus-powered")
        if (v & 0x20) parts.push("remote-wakeup")
        return hex + " (" + parts.join(", ") + ")"
    }

    // Rows for the descriptor section: [{label, value, green}]
    function profileRows() {
        var d = page.dev
        return [
            { label: qsTr("Name"), value: d.name || "–", green: true },
            { label: qsTr("Manufacturer"), value: d.manufacturer || "–", green: true },
            { label: qsTr("Product"), value: d.product || "–", green: true },
            { label: qsTr("Vendor ID"), value: d.vendorId || "–", green: true },
            { label: qsTr("Product ID"), value: d.productId || "–", green: true }
        ]
    }

    function descriptorRows() {
        var d = page.dev
        return [
            { label: qsTr("USB version"), value: (d.usbVersion || "–") },
            { label: qsTr("Device release"), value: (d.bcdDevice || "–") },
            { label: qsTr("Serial"), value: (d.serial && d.serial.length ? d.serial : "–") },
            { label: qsTr("Device class"), value: (d.deviceClass || "00") + " / " + (d.deviceSubClass || "00") + " / " + (d.deviceProtocol || "00") },
            { label: qsTr("EP0 max packet"), value: (d.maxPacketSize0 || "–") },
            { label: qsTr("Speed (Mbit/s)"), value: (d.speedText || "–") },
            { label: qsTr("Configurations"), value: (d.numConfigurations || "–") + " (active #" + (d.configValue || "?") + ")" },
            { label: qsTr("Interfaces"), value: (d.numInterfaces || "–") },
            { label: qsTr("Power"), value: attrText(d.attributes) + ", max " + (d.maxPower || "?") },
            { label: qsTr("Bus / Device"), value: (d.busnum || "?") + " / " + (d.devnum || "?") },
            { label: qsTr("Removable"), value: (d.removable || "–") }
        ]
    }

    function interfaceText(it, idx) {
        var s = "IF " + (it.number !== undefined ? it.number : idx)
              + ": class " + it["class"] + "/" + it.subclass + "/" + it.protocol
        if (it.driver && it.driver.length) s += "  · driver " + it.driver
        var eps = it.endpoints || []
        for (var i = 0; i < eps.length; ++i) {
            var e = eps[i]
            s += "\n   ep " + e.address + "  " + e.type
               + "  " + e.maxPacket + "B  " + e.interval
        }
        return s
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Send to Flipper")
                visible: flipper.connected
                onClicked: flipper.pushJigglerProfile(page.dev, flipper.transportName === "BLE")
            }
            MenuItem {
                text: qsTr("Rename")
                onClicked: {
                    var dlg = pageStack.push(Qt.resolvedUrl("RenameUsbDialog.qml"),
                                             { deviceName: page.dev.name })
                    dlg.accepted.connect(function() {
                        usbLib.renameDevice(page.deviceId, dlg.deviceName)
                    })
                }
            }
            MenuItem {
                text: qsTr("Delete")
                onClicked: {
                    usbLib.removeDevice(page.deviceId)
                    pageStack.pop()
                }
            }
        }

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingSmall

            PageHeader { title: page.dev.name ? page.dev.name : qsTr("Device") }

            // --- Profile (green = sent to the Flipper) --------------------
            SectionHeader { text: qsTr("Sent to Flipper") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: page.sentColor
                text: qsTr("Green fields are written to the jiggler profile.")
            }
            Repeater {
                model: page.profileRows()
                delegate: Row {
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    Label {
                        width: parent.width * 0.40
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        wrapMode: Text.WordWrap
                        text: modelData.label
                    }
                    Label {
                        width: parent.width * 0.60
                        font.pixelSize: Theme.fontSizeSmall
                        color: page.sentColor
                        wrapMode: Text.WordWrap
                        text: modelData.value
                    }
                }
            }

            // --- Identification via usb.ids ------------------------------
            SectionHeader { text: qsTr("Identification") }
            Repeater {
                model: [
                    { label: qsTr("Vendor (usb.ids)"), value: (page.dev.vendorName && page.dev.vendorName.length ? page.dev.vendorName : "–") },
                    { label: qsTr("Product (usb.ids)"), value: (page.dev.productName && page.dev.productName.length ? page.dev.productName : "–") },
                    { label: qsTr("Manufacturer string"), value: (page.dev.manufacturer && page.dev.manufacturer.length ? page.dev.manufacturer : "–") },
                    { label: qsTr("Chip vendor (likely)"), value: (page.dev.chipVendor && page.dev.chipVendor.length ? page.dev.chipVendor : qsTr("unknown")) }
                ]
                delegate: Row {
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    Label {
                        width: parent.width * 0.40
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        wrapMode: Text.WordWrap
                        text: modelData.label
                    }
                    Label {
                        width: parent.width * 0.60
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                        wrapMode: Text.WordWrap
                        text: modelData.value
                    }
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Chip vendor is inferred from the manufacturer string / VID — "
                         + "the exact silicon isn't exposed over USB.")
            }

            // --- Full device descriptor (white) --------------------------
            SectionHeader { text: qsTr("Device descriptor") }
            Repeater {
                model: page.descriptorRows()
                delegate: Row {
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    Label {
                        width: parent.width * 0.40
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                        wrapMode: Text.WordWrap
                        text: modelData.label
                    }
                    Label {
                        width: parent.width * 0.60
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                        wrapMode: Text.WordWrap
                        text: modelData.value
                    }
                }
            }

            // --- Interfaces & endpoints (white) --------------------------
            SectionHeader { text: qsTr("Interfaces & endpoints") }
            Repeater {
                model: page.dev.interfaces || []
                delegate: Label {
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.family: "monospace"
                    color: Theme.primaryColor
                    wrapMode: Text.WordWrap
                    text: page.interfaceText(modelData, index)
                }
            }

            // --- HID report descriptor (white), decoded ------------------
            SectionHeader {
                text: qsTr("HID report descriptor")
                visible: (page.dev.reportDescriptorLen || 0) > 0
            }
            Label {
                visible: (page.dev.reportDescriptorLen || 0) > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: (page.dev.reportDescriptorLen || 0) + " bytes — "
                      + qsTr("the device's report grammar (what it is + byte layout of its reports)")
                wrapMode: Text.WordWrap
            }
            Label {
                id: hidDecoded
                visible: (page.dev.reportDescriptorLen || 0) > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                font.family: "monospace"
                color: Theme.primaryColor
                wrapMode: Text.WrapAnywhere
                text: usbLib.decodeHidReport(page.dev.reportDescriptor || "")
            }
            // Raw hex, collapsed under the decode.
            Label {
                visible: (page.dev.reportDescriptorLen || 0) > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                font.family: "monospace"
                color: Theme.secondaryColor
                opacity: 0.6
                wrapMode: Text.WrapAnywhere
                text: qsTr("raw: ") + (page.dev.reportDescriptor || "")
            }

            // --- Host controller (white) ---------------------------------
            SectionHeader { text: qsTr("Host controller (phone)") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                wrapMode: Text.WordWrap
                text: (page.dev.hostController && page.dev.hostController.length
                       ? page.dev.hostController : qsTr("unknown"))
            }

            Item { width: 1; height: Theme.paddingLarge }

            Label {
                visible: !flipper.connected
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Connect to the Flipper to send this identity to the "
                         + "Mouse-Jiggler-smart app.")
            }
        }

        VerticalScrollDecorator {}
    }
}
