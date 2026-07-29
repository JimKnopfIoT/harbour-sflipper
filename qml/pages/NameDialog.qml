import QtQuick 2.6
import Sailfish.Silica 1.0

// Change the Flipper's display + Bluetooth name (Unleashed NameChanger).
Dialog {
    id: dialog
    property string currentName: ""

    Column {
        width: parent.width
        spacing: Theme.paddingMedium

        DialogHeader { acceptText: qsTr("Save & reboot") }

        TextField {
            id: nameField
            width: parent.width
            label: qsTr("Flipper name")
            placeholderText: qsTr("max 8 chars · empty = reset")
            text: dialog.currentName
            maximumLength: 8
            focus: true
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.WordWrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: qsTr("Sets both the display name and the Bluetooth name. The Flipper "
                     + "reboots to apply. Leave empty to reset to the factory name.\n\n"
                     + "Note: renaming also changes the Bluetooth MAC, so you must "
                     + "pair again in Settings → Bluetooth afterwards.")
        }
    }

    onAccepted: flipper.setDeviceName(nameField.text)
}
