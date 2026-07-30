#ifndef USBLIBRARY_H
#define USBLIBRARY_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <QHash>

// Reads USB descriptors of devices currently attached to the phone (which acts
// as USB host over its USB-C port) from sysfs, and keeps a small persisted
// library of saved devices. Each saved device can later be pushed to the
// Flipper as the USB identity for the "Mouse-Jiggler-smart" app.
//
// No root needed: /sys/bus/usb/devices/*/... attributes are world-readable.
class UsbLibrary : public QObject
{
    Q_OBJECT
    // Saved devices, sorted by class then name. Each entry is a QVariantMap
    // (see parseSysfsDevice() for keys, plus "id", "name", "classKey",
    // "savedAt").
    Q_PROPERTY(QVariantList devices READ devices NOTIFY libraryChanged)
    Q_PROPERTY(int count READ count NOTIFY libraryChanged)

public:
    explicit UsbLibrary(QObject *parent = nullptr);

    QVariantList devices() const;
    int count() const { return m_db.size(); }

    // Scan every non-root USB device currently attached and return the parsed
    // descriptors. Each map carries a "known" flag if it is already saved and a
    // "suggestedName" for the add dialog.
    Q_INVOKABLE QVariantList scanAttached() const;

    // Save a (possibly edited) scanned device map into the library. Uses
    // map["name"] if set, otherwise an auto-generated name. Returns the new id.
    Q_INVOKABLE QString addDevice(const QVariantMap &dev);

    Q_INVOKABLE void renameDevice(const QString &id, const QString &name);
    Q_INVOKABLE void removeDevice(const QString &id);
    Q_INVOKABLE QVariantMap device(const QString &id) const;

    // Decode a HID report descriptor (space-separated hex) into human-readable,
    // indented lines (Usage Page/Usage names, collections, Input/Output flags).
    Q_INVOKABLE QString decodeHidReport(const QString &hex) const;

    // Canonical class label key -> a stable string for QML to localise.
    static QString classifyKey(const QVariantMap &dev);
    // Sort rank for a class key (mice first, then keyboards, ...).
    static int classRank(const QString &key);

signals:
    void libraryChanged();
    void scanError(const QString &message);

private:
    QVariantMap parseSysfsDevice(const QString &sysPath) const;
    QString autoName(const QVariantMap &dev) const;
    static QString matchKey(const QVariantMap &dev);   // vid:pid:serial identity

    // usb.ids lookup (VID->vendor, VID:PID->product) + chip-vendor inference.
    void loadUsbIds() const;
    QString vendorName(const QString &vid) const;
    QString productName(const QString &vid, const QString &pid) const;
    static QString inferChipVendor(const QString &manufacturer, const QString &vendorName);

    mutable bool m_usbIdsLoaded = false;
    mutable QHash<QString, QString> m_usbVendor;
    mutable QHash<QString, QString> m_usbProduct;

    void load();
    void save() const;
    QString dbPath() const;
    QString dataDir() const;

    QList<QVariantMap> m_db;
};

#endif // USBLIBRARY_H
