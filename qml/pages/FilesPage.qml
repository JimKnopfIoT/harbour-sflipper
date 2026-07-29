import QtQuick 2.0
import Sailfish.Silica 1.0

// SD-card / internal-storage browser (Storage RPC). Also the entry point for
// M5: "Open" launches the matching Flipper app on a saved signal file.
Page {
    id: page
    allowedOrientations: Orientation.All

    property string cwd: "/ext"
    property bool loading: false

    function reload() {
        entries.clear()
        loading = true
        flipper.listDir(cwd)
    }

    function enterDir(name) {
        cwd = (cwd === "/" ? "" : cwd) + "/" + name
        reload()
    }

    function goUp() {
        var i = cwd.lastIndexOf("/")
        cwd = i > 0 ? cwd.substring(0, i) : "/"
        reload()
    }

    // Map a saved-signal extension to the Flipper app that opens it (M5).
    function appForFile(name) {
        var n = name.toLowerCase()
        if (n.indexOf(".sub") >= 0)  return "Sub-GHz"
        if (n.indexOf(".ir") >= 0)   return "Infrared"
        if (n.indexOf(".nfc") >= 0)  return "NFC"
        if (n.indexOf(".rfid") >= 0) return "125 kHz RFID"
        if (n.indexOf(".ibtn") >= 0) return "iButton"
        return ""
    }

    Component.onCompleted: reload()

    ListModel { id: entries }

    Connections {
        target: flipper
        onListDirEntry: entries.append({ "name": name, "isDir": isDir, "size": size })
        onListDirDone: page.loading = false
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: entries

        header: Column {
            width: list.width
            PageHeader { title: qsTr("Files") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: page.cwd
                truncationMode: TruncationMode.Fade
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            // "../" — go up one level (hidden at the absolute root)
            BackgroundItem {
                width: list.width
                visible: page.cwd !== "/"
                onClicked: page.goUp()
                Row {
                    x: Theme.horizontalPageMargin
                    height: parent.height
                    spacing: Theme.paddingMedium
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "⬆"
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "../"
                        color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                }
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Internal (/int)")
                onClicked: { page.cwd = "/int"; page.reload() }
            }
            MenuItem {
                text: qsTr("SD card (/ext)")
                onClicked: { page.cwd = "/ext"; page.reload() }
            }
            MenuItem {
                text: qsTr("Reload")
                onClicked: page.reload()
            }
        }

        delegate: ListItem {
            id: item
            width: list.width

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Open on Flipper")
                    visible: !isDir && page.appForFile(name) !== ""
                    onClicked: flipper.openFileInApp(page.appForFile(name),
                                                     (page.cwd === "/" ? "" : page.cwd) + "/" + name)
                }
                MenuItem {
                    text: qsTr("Install firmware update")
                    visible: !isDir && name.toLowerCase() === "update.fuf"
                    onClicked: item.remorseAction(qsTr("Installing update — Flipper will reboot"),
                        function() {
                            flipper.installUpdate((page.cwd === "/" ? "" : page.cwd) + "/" + name)
                        })
                }
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: item.remorseAction(qsTr("Deleting"), function() {
                        flipper.deletePath((page.cwd === "/" ? "" : page.cwd) + "/" + name, isDir)
                        entries.remove(index)
                    })
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: item.contentHeight
                spacing: Theme.paddingMedium

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: isDir ? "📁" : "📄"
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - x
                    text: name
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
            }

            onClicked: if (isDir) page.enterDir(name)
        }

        ViewPlaceholder {
            enabled: !page.loading && entries.count === 0
            text: qsTr("Empty")
        }

        VerticalScrollDecorator {}
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: page.loading
        size: BusyIndicatorSize.Medium
    }
}
