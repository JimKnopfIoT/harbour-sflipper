#include "flipper.h"
#include "transport/transport.h"
#include "transport/serialtransport.h"
#include "transport/bletransport.h"

#include <QFile>
#include <QColor>
#include <QTimer>
#include <QDir>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// Screen image provider
// ---------------------------------------------------------------------------
QImage ScreenImageProvider::requestImage(const QString &id, QSize *size, const QSize &requested)
{
    Q_UNUSED(id)
    QImage img = m_image;
    if (img.isNull()) {
        img = QImage(128, 64, QImage::Format_RGB32);
        img.fill(QColor(0xff, 0x88, 0x11));      // Flipper-orange backlight
    }
    if (size)
        *size = img.size();
    if (requested.width() > 0 && requested.height() > 0)
        return img.scaled(requested, Qt::KeepAspectRatio, Qt::FastTransformation);
    return img;
}

// ---------------------------------------------------------------------------
// Flipper facade
// ---------------------------------------------------------------------------
Flipper::Flipper(ScreenImageProvider *screenProvider, QObject *parent)
    : QObject(parent), m_screenProvider(screenProvider)
{
    m_statusText = tr("Not connected");

    connect(&m_rpc, &RpcSession::deviceInfoPair, this, &Flipper::onDeviceInfoPair);
    connect(&m_rpc, &RpcSession::deviceInfoComplete, this, &Flipper::onDeviceInfoComplete);
    connect(&m_rpc, &RpcSession::powerInfoPair, this, &Flipper::onPowerInfoPair);
    connect(&m_rpc, &RpcSession::powerInfoComplete, this, &Flipper::onPowerInfoComplete);
    connect(&m_rpc, &RpcSession::screenFrame, this, &Flipper::onScreenFrame);
    connect(&m_rpc, &RpcSession::storageListEntry, this, &Flipper::onListEntry);
    connect(&m_rpc, &RpcSession::storageListComplete, this, &Flipper::onListComplete);
    connect(&m_rpc, &RpcSession::updateResult, this, &Flipper::onUpdateResult);
    connect(&m_rpc, &RpcSession::commandComplete, this, &Flipper::onCommandComplete);
    connect(&m_rpc, &RpcSession::protocolError, this, &Flipper::onProtocolError);

    m_uploadTimer = new QTimer(this);
    m_uploadTimer->setInterval(0);   // send chunks as fast as USB back-pressure allows
    connect(m_uploadTimer, &QTimer::timeout, this, &Flipper::onUploadTick);
}

QString Flipper::transportName() const
{
    return m_transport ? m_transport->name() : QString();
}

int Flipper::batteryPercent() const
{
    for (const char *key : {"charge_level", "battery_level", "charge"}) {
        if (m_powerInfo.contains(key))
            return m_powerInfo.value(key).toInt();
    }
    return -1;
}

void Flipper::setConnected(bool c)
{
    if (m_connected == c)
        return;
    m_connected = c;
    emit connectedChanged();
}

void Flipper::setStatus(const QString &s)
{
    if (m_statusText == s)
        return;
    m_statusText = s;
    emit statusTextChanged();
}

// --- Connection ------------------------------------------------------------
void Flipper::connectUsb(const QString &device)
{
    teardownTransport();
    setStatus(tr("Opening %1…").arg(device));
    SerialTransport *t = new SerialTransport(device, this);
    m_transport = t;
    connect(t, &Transport::ready, this, &Flipper::onTransportReady);
    connect(t, &Transport::closed, this, &Flipper::onTransportClosed);
    connect(t, &Transport::errorOccurred, this, &Flipper::onTransportError);
    m_rpc.attach(t);
    t->open();
}

void Flipper::connectBle()
{
    teardownTransport();
    setStatus(tr("Connecting via BLE…"));
    BleTransport *t = new BleTransport(this);
    m_transport = t;
    connect(t, &Transport::ready, this, &Flipper::onTransportReady);
    connect(t, &Transport::closed, this, &Flipper::onTransportClosed);
    connect(t, &Transport::errorOccurred, this, &Flipper::onTransportError);
    m_rpc.attach(t);
    t->open();
}

void Flipper::disconnectDevice()
{
    if (m_screenStreaming)
        stopScreen();
    teardownTransport();
    setConnected(false);
    setStatus(tr("Not connected"));
}

void Flipper::teardownTransport()
{
    if (m_transport) {
        m_transport->close();
        m_transport->deleteLater();
        m_transport = nullptr;
    }
    m_rpc.attach(nullptr);
}

void Flipper::onTransportReady()
{
    setConnected(true);
    setStatus(tr("Connected via %1").arg(transportName()));
    refreshInfo();
}

void Flipper::onTransportClosed()
{
    setConnected(false);
    m_screenStreaming = false;
    emit screenStreamingChanged();
    setStatus(tr("Disconnected"));
}

void Flipper::onTransportError(const QString &message)
{
    setStatus(message);
    emit actionError(message);
}

void Flipper::refreshInfo()
{
    if (!m_connected)
        return;
    m_deviceInfoPending.clear();
    m_powerInfoPending.clear();
    m_rpc.requestDeviceInfo();
    m_rpc.requestPowerInfo();
}

// --- Info streams ----------------------------------------------------------
void Flipper::onDeviceInfoPair(const QString &key, const QString &value)
{
    m_deviceInfoPending.insert(key, value);
}

void Flipper::onDeviceInfoComplete()
{
    m_deviceInfo = m_deviceInfoPending;
    emit deviceInfoChanged();
}

void Flipper::onPowerInfoPair(const QString &key, const QString &value)
{
    m_powerInfoPending.insert(key, value);
}

void Flipper::onPowerInfoComplete()
{
    m_powerInfo = m_powerInfoPending;
    emit powerInfoChanged();
}

// --- Screen / input --------------------------------------------------------
void Flipper::startScreen()
{
    if (!m_connected)
        return;
    m_rpc.startScreenStream();
    m_screenStreaming = true;
    emit screenStreamingChanged();
}

void Flipper::stopScreen()
{
    if (!m_connected)
        return;
    m_rpc.stopScreenStream();
    m_screenStreaming = false;
    emit screenStreamingChanged();
}

void Flipper::pressKey(int key)
{
    // Emulate a short tap: PRESS → SHORT → RELEASE.
    m_rpc.sendInput(key, RpcSession::Press);
    m_rpc.sendInput(key, RpcSession::Short);
    m_rpc.sendInput(key, RpcSession::Release);
}

void Flipper::sendInput(int key, int type)
{
    m_rpc.sendInput(key, type);
}

void Flipper::onScreenFrame(const QByteArray &data, int orientation)
{
    Q_UNUSED(orientation)
    if (!m_screenProvider || data.size() < 1024)
        return;

    // 128x64, 1bpp, SSD1306 page layout: byte = x + (y/8)*128, bit = y%8.
    // Lit pixel → dark on the LCD; background → orange backlight.
    QImage img(128, 64, QImage::Format_RGB32);
    const QRgb on = qRgb(0x2b, 0x1a, 0x00);
    const QRgb off = qRgb(0xff, 0x88, 0x11);
    const uchar *p = reinterpret_cast<const uchar *>(data.constData());
    for (int y = 0; y < 64; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const int page = (y >> 3) * 128;
        const uchar mask = uchar(1 << (y & 7));
        for (int x = 0; x < 128; ++x)
            line[x] = (p[page + x] & mask) ? on : off;
    }
    m_screenProvider->setImage(img);
    ++m_screenRevision;
    emit screenUpdated();
}

// --- File manager ----------------------------------------------------------
void Flipper::listDir(const QString &path)
{
    if (m_connected)
        m_rpc.storageList(path);
}

void Flipper::deletePath(const QString &path, bool recursive)
{
    if (m_connected)
        m_rpc.storageDelete(path, recursive);
}

void Flipper::makeDir(const QString &path)
{
    if (m_connected)
        m_rpc.storageMkdir(path);
}

void Flipper::uploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected)
        return;
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit actionError(tr("Cannot read %1").arg(localPath));
        return;
    }
    // NOTE: single-shot write. Large files need chunked writes (has_next) —
    // added when the file manager grows up in M4.
    const QByteArray data = f.readAll();
    m_rpc.storageWrite(remotePath, data);
}

void Flipper::onListEntry(quint32 cmdId, const QString &name, bool isDir, quint32 size)
{
    Q_UNUSED(cmdId)
    emit listDirEntry(name, isDir, int(size));
}

void Flipper::onListComplete(quint32 cmdId)
{
    Q_UNUSED(cmdId)
    emit listDirDone();
}

// --- Saved signals ---------------------------------------------------------
void Flipper::openFileInApp(const QString &appName, const QString &remotePath)
{
    if (m_connected)
        m_rpc.appStart(appName, remotePath);
}

void Flipper::appStart(const QString &name, const QString &args)
{
    if (m_connected)
        m_rpc.appStart(name, args);
}

void Flipper::reboot(int mode)
{
    if (m_connected)
        m_rpc.systemReboot(mode);
}

void Flipper::setDeviceName(const QString &name)
{
    if (!m_connected)
        return;
    const QString path = QStringLiteral("/ext/dolphin/name.settings");
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        m_rpc.storageDelete(path, false);         // reset to factory name
    } else {
        // flipper_format file: header + Name key (see Unleashed NameChanger).
        const QByteArray content = QByteArrayLiteral("Filetype: Flipper Name File\n"
                                                     "Version: 1\n"
                                                     "Name: ") + trimmed.left(8).toUtf8()
                                   + QByteArrayLiteral("\n");
        m_rpc.storageWrite(path, content);
    }
    setStatus(tr("Name saved — rebooting Flipper…"));
    // Let the write/delete flush, then reboot so the new name is applied.
    QTimer::singleShot(900, this, [this]() {
        if (m_connected)
            m_rpc.systemReboot(0);
    });
}

void Flipper::installUpdate(const QString &manifestPath)
{
    if (!m_connected)
        return;
    setStatus(tr("Validating update…"));
    m_rpc.systemUpdate(manifestPath);
}

static QString updateCodeText(int code)
{
    switch (code) {
    case 1: return QStringLiteral("manifest path invalid");
    case 2: return QStringLiteral("manifest folder not found");
    case 3: return QStringLiteral("manifest invalid");
    case 4: return QStringLiteral("a stage file is missing");
    case 5: return QStringLiteral("stage integrity error");
    case 6: return QStringLiteral("manifest pointer error");
    case 7: return QStringLiteral("firmware target mismatch");
    case 8: return QStringLiteral("outdated manifest version");
    default: return QStringLiteral("error code %1").arg(code);
    }
}

void Flipper::onUpdateResult(quint32 cmdId, int code)
{
    Q_UNUSED(cmdId)
    if (m_uploadActive) {
        m_uploadActive = false;
        emit uploadChanged();
    }
    if (code == 0) {
        setStatus(tr("Update accepted — rebooting to install…"));
        setUploadStatus(tr("Rebooting to install…"));
        emit uploadChanged();
        m_rpc.systemReboot(2);   // RebootMode UPDATE
    } else {
        setStatus(tr("Update rejected"));
        setUploadStatus(tr("Rejected: %1").arg(updateCodeText(code)));
        emit uploadChanged();
        emit actionError(tr("Firmware update rejected: %1").arg(updateCodeText(code)));
    }
}

// --- Firmware upload (Stufe 2) ---------------------------------------------
QVariantList Flipper::updatePackages()
{
    QVariantList out;
    const QString dl = QDir::homePath() + QStringLiteral("/Downloads");
    QDir d(dl);
    const QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &sub : dirs) {
        const QString path = dl + QLatin1Char('/') + sub;
        if (QFileInfo::exists(path + QStringLiteral("/update.fuf"))) {
            QVariantMap m;
            m.insert(QStringLiteral("name"), sub);
            m.insert(QStringLiteral("path"), path);
            out.append(m);
        }
    }
    return out;
}

void Flipper::setUploadStatus(const QString &s)
{
    m_uploadStatus = s;
}

void Flipper::uploadUpdatePackage(const QString &localDir)
{
    if (!m_connected) { emit actionError(tr("Not connected")); return; }
    if (m_uploadActive) return;
    if (transportName() != QLatin1String("USB-C")) {
        emit actionError(tr("Firmware upload needs a USB-C connection (much faster "
                            "and reliable than BLE for ~14 MB)."));
        return;
    }
    QDir d(localDir);
    if (!QFileInfo::exists(localDir + QStringLiteral("/update.fuf"))) {
        emit actionError(tr("No update.fuf in that folder."));
        return;
    }

    m_uploadFiles.clear();
    m_uploadTotal = 0;
    const QStringList files = d.entryList(QDir::Files, QDir::Name);
    for (const QString &f : files) {
        const QString p = localDir + QLatin1Char('/') + f;
        m_uploadFiles.append(p);
        m_uploadTotal += QFileInfo(p).size();
    }
    if (m_uploadFiles.isEmpty()) { emit actionError(tr("Empty package")); return; }

    m_uploadRemoteDir = QStringLiteral("/ext/update/") + d.dirName();
    m_uploadIndex = 0;
    m_uploadSent = 0;
    m_uploadActive = true;
    setUploadStatus(tr("Preparing…"));
    emit uploadChanged();

    m_rpc.storageMkdir(QStringLiteral("/ext/update"));
    m_rpc.storageMkdir(m_uploadRemoteDir);
    startNextUploadFile();
}

void Flipper::startNextUploadFile()
{
    if (m_uploadFile) { m_uploadFile->close(); delete m_uploadFile; m_uploadFile = nullptr; }

    if (m_uploadIndex >= m_uploadFiles.size()) {
        setUploadStatus(tr("Installing…"));
        emit uploadChanged();
        m_rpc.systemUpdate(m_uploadRemoteDir + QStringLiteral("/update.fuf"));
        return;
    }

    const QString local = m_uploadFiles.at(m_uploadIndex);
    m_uploadFile = new QFile(local);
    if (!m_uploadFile->open(QIODevice::ReadOnly)) {
        finishUpload(false, tr("Cannot read %1").arg(local));
        return;
    }
    m_uploadRemotePath = m_uploadRemoteDir + QLatin1Char('/') + QFileInfo(local).fileName();
    m_uploadCmdId = m_rpc.allocCommandId();
    setUploadStatus(tr("Uploading %1 (%2/%3)…").arg(QFileInfo(local).fileName())
                    .arg(m_uploadIndex + 1).arg(m_uploadFiles.size()));
    emit uploadChanged();
    m_uploadTimer->start();
}

void Flipper::onUploadTick()
{
    if (!m_uploadActive || !m_uploadFile) { m_uploadTimer->stop(); return; }

    const int kChunk = 1024;
    const QByteArray chunk = m_uploadFile->read(kChunk);
    const bool atEnd = m_uploadFile->atEnd();
    m_rpc.storageWriteChunk(m_uploadCmdId, m_uploadRemotePath, chunk, !atEnd);

    const qint64 before = m_uploadSent;
    m_uploadSent += chunk.size();
    // Throttle progress notifications to ~every 128 KB.
    if (atEnd || (m_uploadSent >> 17) != (before >> 17))
        emit uploadChanged();

    if (atEnd)
        m_uploadTimer->stop();   // wait for command_complete → next file
}

void Flipper::onCommandComplete(quint32 cmdId, int status)
{
    if (!m_uploadActive || cmdId != m_uploadCmdId)
        return;   // e.g. mkdir / other acks — ignore
    if (status != 0) {
        finishUpload(false, tr("Upload failed (storage error %1)").arg(status));
        return;
    }
    m_uploadIndex++;
    startNextUploadFile();
}

void Flipper::finishUpload(bool ok, const QString &message)
{
    if (m_uploadTimer)
        m_uploadTimer->stop();
    if (m_uploadFile) { m_uploadFile->close(); delete m_uploadFile; m_uploadFile = nullptr; }
    m_uploadActive = false;
    setUploadStatus(message);
    emit uploadChanged();
    if (!ok)
        emit actionError(message);
}

void Flipper::cancelUpload()
{
    if (!m_uploadActive)
        return;
    // Close the in-flight write command so the Flipper isn't left waiting.
    if (m_uploadCmdId)
        m_rpc.storageWriteChunk(m_uploadCmdId, m_uploadRemotePath, QByteArray(), false);
    finishUpload(false, tr("Upload cancelled"));
}

void Flipper::onProtocolError(const QString &message)
{
    emit actionError(message);
}
