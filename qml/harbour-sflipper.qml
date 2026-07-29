import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import "pages"

ApplicationWindow {
    initialPage: Component { MainPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: Orientation.All

    // App-wide, visible feedback for backend errors (BLE, uploads, updates …).
    Notification {
        id: errorBanner
        isTransient: true
        appName: "sFlipper"
    }
    Connections {
        target: flipper
        onActionError: {
            errorBanner.close()
            errorBanner.previewSummary = "sFlipper"
            errorBanner.previewBody = message
            errorBanner.publish()
        }
    }
}
