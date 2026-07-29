import QtQuick 2.6
import Sailfish.Silica 1.0

// Stufe 2: upload a local update-package folder (from Downloads) to the Flipper
// and install it. Requires USB-C (fast, reliable back-pressure for ~14 MB).
Page {
    id: page
    allowedOrientations: Orientation.All

    property var packages: []
    function reload() { packages = flipper.updatePackages() }
    Component.onCompleted: reload()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height

        PullDownMenu {
            MenuItem { text: qsTr("Reload"); onClicked: page.reload() }
        }

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Firmware update") }

            // --- progress / status ---
            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: flipper.uploadActive || flipper.uploadStatus.length > 0

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.WordWrap
                    color: Theme.highlightColor
                    text: flipper.uploadStatus
                }
                ProgressBar {
                    width: parent.width
                    visible: flipper.uploadActive
                    value: flipper.uploadProgress
                    valueText: Math.round(flipper.uploadProgress * 100) + " %"
                    label: qsTr("Uploading")
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Cancel")
                    visible: flipper.uploadActive
                    onClicked: flipper.cancelUpload()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Pick an update package from Downloads (a folder containing "
                         + "update.fuf) to upload and install. Requires a USB-C connection. "
                         + "The Flipper reboots to apply.")
            }

            // Prominent hint when not on USB-C (upload is USB-only).
            Label {
                visible: flipper.transportName !== "USB-C"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: "#FF8200"
                font.bold: true
                text: qsTr("⚠ Connect via USB-C to upload firmware. Plug in the "
                         + "USB-C cable and use “Connect via USB-C”. "
                         + "(Currently: " + (flipper.transportName.length ? flipper.transportName : "not connected") + ")")
            }

            SectionHeader { text: qsTr("Packages in Downloads") }

            Repeater {
                model: page.packages
                delegate: ListItem {
                    width: page.width
                    enabled: !flipper.uploadActive && flipper.transportName === "USB-C"
                    onClicked: remorseAction(qsTr("Uploading & installing — Flipper reboots"),
                                             function() { flipper.uploadUpdatePackage(modelData.path) })
                    Label {
                        x: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        truncationMode: TruncationMode.Fade
                        text: modelData.name
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
            }

            Label {
                visible: page.packages.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                text: qsTr("No packages found. Put an extracted update folder "
                         + "(with update.fuf) into Downloads.")
            }
        }
    }
}
