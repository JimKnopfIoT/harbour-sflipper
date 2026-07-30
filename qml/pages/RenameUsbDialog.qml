import QtQuick 2.6
import Sailfish.Silica 1.0

Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    // In/out: the edited name. Caller reads it back on `accepted`.
    property string deviceName: ""
    property string heading: qsTr("Rename")

    canAccept: nameField.text.trim().length > 0
    onAccepted: deviceName = nameField.text.trim()

    Column {
        width: parent.width

        DialogHeader { title: dialog.heading }

        TextField {
            id: nameField
            width: parent.width
            text: dialog.deviceName
            label: qsTr("Device name")
            placeholderText: qsTr("e.g. Logitech-Laptop")
            focus: true
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: if (dialog.canAccept) dialog.accept()
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            text: qsTr("Tip: with two similar devices, name them e.g. …-Laptop and …-PC.")
        }
    }
}
