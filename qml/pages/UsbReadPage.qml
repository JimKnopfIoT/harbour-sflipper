import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property var found: []

    function rescan() {
        found = usbLib.scanAttached()
    }

    Component.onCompleted: rescan()

    SilicaListView {
        id: list
        anchors.fill: parent
        model: page.found

        header: Column {
            width: list.width
            PageHeader { title: qsTr("Read attached device") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Devices currently plugged into the phone. Tap one to name "
                         + "and save it to your library.")
            }
            Item { width: 1; height: Theme.paddingLarge }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Rescan")
                onClicked: page.rescan()
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("Nothing attached")
            hintText: qsTr("Plug a USB device into the phone (an OTG adapter switches "
                         + "the port to host mode), then pull down to rescan.")
        }

        delegate: ListItem {
            id: item
            width: list.width
            contentHeight: Theme.itemSizeMedium
            property var dev: modelData

            onClicked: {
                var dlg = pageStack.push(Qt.resolvedUrl("RenameUsbDialog.qml"),
                                         { deviceName: item.dev.suggestedName,
                                           heading: qsTr("Save device") })
                dlg.accepted.connect(function() {
                    var d = item.dev
                    d.name = dlg.deviceName
                    usbLib.addDevice(d)
                    pageStack.pop()   // back to the library, which now lists it
                })
            }

            Column {
                anchors {
                    left: parent.left; leftMargin: Theme.horizontalPageMargin
                    right: parent.right; rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: item.dev.suggestedName
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: item.dev.vendorId + ":" + item.dev.productId
                          + (item.dev.known ? "  ·  " + qsTr("already saved") : "")
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
