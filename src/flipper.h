#ifndef FLIPPER_H
#define FLIPPER_H

#include <QObject>
#include <QVariantMap>
#include <QImage>
#include <QQuickImageProvider>

#include "rpc/rpcsession.h"

class Transport;
class QFile;
class QTimer;

// Serves the live Flipper screen (128x64) to QML via image://flipperscreen/.
class ScreenImageProvider : public QQuickImageProvider
{
public:
    ScreenImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override;
    void setImage(const QImage &img) { m_image = img; }
private:
    QImage m_image;
};

// High-level facade exposed to QML as the context property `flipper`.
// Owns the active Transport (USB-C or BLE) and the RpcSession, translates RPC
// signals into QML-friendly properties and invokable actions.
class Flipper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString transportName READ transportName NOTIFY connectedChanged)
    Q_PROPERTY(QVariantMap deviceInfo READ deviceInfo NOTIFY deviceInfoChanged)
    Q_PROPERTY(QVariantMap powerInfo READ powerInfo NOTIFY powerInfoChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString hardwareModel READ hardwareModel NOTIFY deviceInfoChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY powerInfoChanged)
    Q_PROPERTY(int screenRevision READ screenRevision NOTIFY screenUpdated)
    Q_PROPERTY(bool screenStreaming READ screenStreaming NOTIFY screenStreamingChanged)
    Q_PROPERTY(bool uploadActive READ uploadActive NOTIFY uploadChanged)
    Q_PROPERTY(qreal uploadProgress READ uploadProgress NOTIFY uploadChanged)
    Q_PROPERTY(QString uploadStatus READ uploadStatus NOTIFY uploadChanged)

public:
    explicit Flipper(ScreenImageProvider *screenProvider = nullptr, QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString statusText() const { return m_statusText; }
    QString transportName() const;
    QVariantMap deviceInfo() const { return m_deviceInfo; }
    QVariantMap powerInfo() const { return m_powerInfo; }
    QString deviceName() const { return m_deviceInfo.value("hardware_name").toString(); }
    QString firmwareVersion() const { return m_deviceInfo.value("firmware_version").toString(); }
    QString hardwareModel() const { return m_deviceInfo.value("hardware_model").toString(); }
    int batteryPercent() const;
    int screenRevision() const { return m_screenRevision; }
    bool screenStreaming() const { return m_screenStreaming; }

    // Connection
    Q_INVOKABLE void connectUsb(const QString &device = QStringLiteral("/dev/ttyACM0"));
    Q_INVOKABLE void connectBle();          // BLE backend (BlueZ) — added in M1/BLE
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void refreshInfo();

    // Remote control (M3)
    Q_INVOKABLE void startScreen();
    Q_INVOKABLE void stopScreen();
    Q_INVOKABLE void pressKey(int key);     // short press: press+release
    Q_INVOKABLE void sendInput(int key, int type);

    // File manager (M4)
    Q_INVOKABLE void listDir(const QString &path);
    Q_INVOKABLE void deletePath(const QString &path, bool recursive);
    Q_INVOKABLE void makeDir(const QString &path);
    Q_INVOKABLE void uploadFile(const QString &localPath, const QString &remotePath);

    // Saved signals (M5)
    Q_INVOKABLE void openFileInApp(const QString &appName, const QString &remotePath);
    Q_INVOKABLE void appStart(const QString &name, const QString &args);

    // Write a saved USB device profile to the SD for the Mouse-Jiggler-smart
    // .fap (/ext/apps_data/mouse_jiggler_smart/id.conf). If start is true, also
    // launch the app — only sensible over BLE, since starting it over USB turns
    // the Flipper into a HID device and drops our USB-RPC serial link.
    Q_INVOKABLE void pushJigglerProfile(const QVariantMap &dev, bool start);

    Q_INVOKABLE void reboot(int mode);

    // Change the Flipper's name (display + BT). Empty name resets to default.
    // Unleashed stores it in /ext/dolphin/name.settings; a reboot applies it.
    Q_INVOKABLE void setDeviceName(const QString &name);

    // Install a firmware update already present on the SD card: validate the
    // manifest (system_update), then reboot into the updater on success.
    Q_INVOKABLE void installUpdate(const QString &manifestPath);

    // Firmware upload (Stufe 2): push a local update-package folder to the SD
    // (/ext/update/<name>/) with progress, then install it.
    bool uploadActive() const { return m_uploadActive; }
    qreal uploadProgress() const { return m_uploadTotal > 0 ? qreal(m_uploadSent) / m_uploadTotal : 0; }
    QString uploadStatus() const { return m_uploadStatus; }
    Q_INVOKABLE QVariantList updatePackages();     // folders in Downloads with update.fuf
    Q_INVOKABLE void uploadUpdatePackage(const QString &localDir);
    Q_INVOKABLE void cancelUpload();

    // Install the bundled Mouse-Jiggler-smart .fap onto the SD
    // (/ext/apps/USB/). Reuses the chunked uploader; USB-C only.
    Q_INVOKABLE void installJigglerFap();

signals:
    void connectedChanged();
    void statusTextChanged();
    void deviceInfoChanged();
    void powerInfoChanged();
    void screenUpdated();
    void screenStreamingChanged();
    void uploadChanged();
    void listDirEntry(const QString &name, bool isDir, int size);
    void listDirDone();
    void actionError(const QString &message);
    void actionInfo(const QString &message);   // non-error, transient feedback

private slots:
    void onTransportReady();
    void onTransportClosed();
    void onTransportError(const QString &message);
    void onDeviceInfoPair(const QString &key, const QString &value);
    void onDeviceInfoComplete();
    void onPowerInfoPair(const QString &key, const QString &value);
    void onPowerInfoComplete();
    void onScreenFrame(const QByteArray &data, int orientation);
    void onListEntry(quint32 cmdId, const QString &name, bool isDir, quint32 size);
    void onListComplete(quint32 cmdId);
    void onUpdateResult(quint32 cmdId, int code);
    void onCommandComplete(quint32 cmdId, int status);
    void onUploadTick();
    void onProtocolError(const QString &message);

private:
    void setConnected(bool c);
    void setStatus(const QString &s);
    void teardownTransport();

    ScreenImageProvider *m_screenProvider = nullptr;
    Transport *m_transport = nullptr;
    RpcSession m_rpc;

    bool m_connected = false;
    bool m_screenStreaming = false;
    QString m_statusText;
    QVariantMap m_deviceInfo;
    QVariantMap m_deviceInfoPending;
    QVariantMap m_powerInfo;
    QVariantMap m_powerInfoPending;
    int m_screenRevision = 0;

    // --- firmware upload state ---
    void startNextUploadFile();
    void finishUpload(bool ok, const QString &message);
    void setUploadStatus(const QString &s);

    // Confirming the jiggler-profile write (id.conf) on its command_complete.
    quint32 m_profileWriteCmdId = 0;
    QString m_lastProfileName;

    bool m_uploadActive = false;
    bool m_uploadIsFirmware = true;  // false => plain file copy (e.g. the fap)
    QString m_uploadStatus;
    QStringList m_uploadFiles;       // local file paths
    QString m_uploadRemoteDir;       // /ext/update/<name>
    QString m_uploadRemotePath;      // current file's remote path
    int m_uploadIndex = 0;
    QFile *m_uploadFile = nullptr;
    quint32 m_uploadCmdId = 0;
    qint64 m_uploadTotal = 0;
    qint64 m_uploadSent = 0;
    QTimer *m_uploadTimer = nullptr;
};

#endif // FLIPPER_H
