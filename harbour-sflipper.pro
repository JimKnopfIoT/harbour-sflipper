# harbour-sflipper — native SailfishOS client for the Flipper Zero.
#
# Milestones (see PROTOCOL.md):
#   M0  scaffold                                — builds empty
#   M1  transport (USB-CDC + BlueZ) + RPC layer — this
#   M2  device info + battery   (system RPC)
#   M3  screen streaming + input (gui RPC)
#   M4  file manager             (storage RPC)
#   M5  saved-signal trigger      (app RPC)

TARGET = harbour-sflipper

CONFIG += sailfishapp sailfishapp_i18n c++17

QT += dbus quick gui

# Installed to /usr/share/icons/hicolor/<size>/apps/ (see sailfishapp.prf).
SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# nanopb runtime needs malloc for the FT_POINTER string/bytes fields the
# Flipper .options use (char* key/value/path, pb_bytes_array_t* data).
DEFINES += PB_ENABLE_MALLOC

INCLUDEPATH += \
    src \
    lib/nanopb \
    lib/generated

# --- Bundled protobuf (nanopb runtime + generated Flipper messages) ---------
SOURCES += \
    lib/nanopb/pb_common.c \
    lib/nanopb/pb_encode.c \
    lib/nanopb/pb_decode.c \
    lib/generated/flipper.pb.c \
    lib/generated/system.pb.c \
    lib/generated/storage.pb.c \
    lib/generated/gui.pb.c \
    lib/generated/application.pb.c \
    lib/generated/gpio.pb.c \
    lib/generated/property.pb.c \
    lib/generated/desktop.pb.c

# --- App sources -----------------------------------------------------------
HEADERS += \
    src/flipper.h \
    src/usblibrary.h \
    src/transport/transport.h \
    src/transport/serialtransport.h \
    src/transport/bletransport.h \
    src/rpc/rpcsession.h

SOURCES += \
    src/harbour-sflipper.cpp \
    src/flipper.cpp \
    src/usblibrary.cpp \
    src/transport/serialtransport.cpp \
    src/transport/bletransport.cpp \
    src/rpc/rpcsession.cpp

# NOTE: runs unsandboxed (no [X-Sailjail] in the .desktop) so it can reach
# /dev/ttyACM0 (USB-C) and BlueZ freely. Fine for a self-built / GitHub app;
# a Harbour-store build would instead need a sandbox profile + permissions.

# udev rule: make the Flipper's USB serial node accessible without dialout.
# NB: must be /etc/udev/rules.d — on SailfishOS /lib is not a symlink to
# /usr/lib, and udev does not read /usr/lib/udev/rules.d there.
udevrule.files = data/99-flipper.rules
udevrule.path  = /etc/udev/rules.d
INSTALLS += udevrule

# Bundled Flipper app: the Mouse-Jiggler-smart .fap, installed to the SD by the
# app on request (/ext/apps/USB/). Built separately from the firmware tree.
jigglerfap.files = data/apps/mouse_jiggler_smart.fap
jigglerfap.path  = /usr/share/harbour-sflipper/apps
INSTALLS += jigglerfap

# --- Translations ----------------------------------------------------------
TRANSLATIONS += translations/harbour-sflipper-de.ts
lupdate_only {
    SOURCES += qml/*.qml qml/pages/*.qml qml/cover/*.qml
}

OTHER_FILES += \
    qml/harbour-sflipper.qml \
    qml/pages/MainPage.qml \
    qml/pages/DeviceInfoPage.qml \
    qml/pages/RemotePage.qml \
    qml/pages/FilesPage.qml \
    qml/pages/FlipperKey.qml \
    qml/pages/UsbDevicesPage.qml \
    qml/pages/UsbReadPage.qml \
    qml/pages/UsbDeviceDetailPage.qml \
    qml/pages/RenameUsbDialog.qml \
    qml/cover/CoverPage.qml \
    harbour-sflipper.desktop \
    rpm/harbour-sflipper.spec \
    data/harbour-sflipper.permission \
    PROTOCOL.md
