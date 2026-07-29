import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    function pairs(map) {
        var out = []
        var keys = Object.keys(map).sort()
        for (var i = 0; i < keys.length; i++)
            out.push({ "k": keys[i], "v": String(map[keys[i]]) })
        return out
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Change Flipper name")
                onClicked: pageStack.push(Qt.resolvedUrl("NameDialog.qml"),
                                          { currentName: flipper.deviceName })
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: flipper.refreshInfo()
            }
        }

        Column {
            id: column
            width: page.width
            spacing: 0

            PageHeader { title: qsTr("Device info") }

            SectionHeader { text: qsTr("System") }
            Repeater {
                model: page.pairs(flipper.deviceInfo)
                DetailItem { label: modelData.k; value: modelData.v }
            }

            SectionHeader { text: qsTr("Power") }
            Repeater {
                model: page.pairs(flipper.powerInfo)
                DetailItem { label: modelData.k; value: modelData.v }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}
