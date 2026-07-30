// harbour-sflipper — native SailfishOS client for the Flipper Zero.
#include <QtQuick>
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <sailfishapp.h>

#include "flipper.h"
#include "usblibrary.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    // The screen provider is owned by the QML engine; Flipper keeps a raw
    // pointer to push live frames into it.
    ScreenImageProvider *screen = new ScreenImageProvider();
    view->engine()->addImageProvider(QStringLiteral("flipperscreen"), screen);

    Flipper flipper(screen);
    view->rootContext()->setContextProperty(QStringLiteral("flipper"), &flipper);

    // USB device library (read attached HID devices, save/rename, push to Flipper).
    UsbLibrary usbLib;
    view->rootContext()->setContextProperty(QStringLiteral("usbLib"), &usbLib);

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-sflipper.qml")));
    view->show();

    return app->exec();
}
