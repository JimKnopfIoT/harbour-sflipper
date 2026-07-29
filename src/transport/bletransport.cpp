#include "bletransport.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>
#include <QDebug>

namespace {
const QString kBluez            = QStringLiteral("org.bluez");
const QString kObjMgrIface      = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kPropsIface       = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kDeviceIface      = QStringLiteral("org.bluez.Device1");
const QString kGattCharIface    = QStringLiteral("org.bluez.GattCharacteristic1");

// Serial service characteristic UUIDs.
const QString kUuidRx = QStringLiteral("19ed82ae-ed21-4c9d-4145-228e62fe0000"); // write
const QString kUuidTx = QStringLiteral("19ed82ae-ed21-4c9d-4145-228e61fe0000"); // indicate
}

typedef QMap<QString, QVariantMap> InterfaceList;
typedef QMap<QDBusObjectPath, InterfaceList> ManagedObjectList;
Q_DECLARE_METATYPE(InterfaceList)
Q_DECLARE_METATYPE(ManagedObjectList)

BleTransport::BleTransport(QObject *parent) : Transport(parent)
{
    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();
}

BleTransport::~BleTransport()
{
    close();
}

void BleTransport::fail(const QString &msg)
{
    emit errorOccurred(msg);
    close();
}

bool BleTransport::findDevice()
{
    QDBusInterface om(kBluez, QStringLiteral("/"), kObjMgrIface, QDBusConnection::systemBus());
    QDBusReply<ManagedObjectList> reply = om.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid())
        return false;

    const ManagedObjectList objects = reply.value();

    // 1) locate the bonded Flipper device
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const InterfaceList &ifaces = it.value();
        if (!ifaces.contains(kDeviceIface))
            continue;
        const QVariantMap dev = ifaces.value(kDeviceIface);
        // Only bonded Flippers — a rename changes the BLE MAC, so a stale
        // (unpaired) object with the old name may still linger.
        if (!dev.value(QStringLiteral("Paired")).toBool())
            continue;
        const QString alias = dev.value(QStringLiteral("Alias")).toString();
        const QString nm = dev.value(QStringLiteral("Name")).toString();
        if (alias.contains(QStringLiteral("Flipper"), Qt::CaseInsensitive)
            || nm.contains(QStringLiteral("Flipper"), Qt::CaseInsensitive)) {
            m_devicePath = it.key().path();
            break;
        }
    }
    if (m_devicePath.isEmpty())
        return false;

    // 2) find its RX/TX characteristics (by UUID, under the device path)
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QString path = it.key().path();
        if (!path.startsWith(m_devicePath + QLatin1Char('/')))
            continue;
        const InterfaceList &ifaces = it.value();
        if (!ifaces.contains(kGattCharIface))
            continue;
        const QString uuid = ifaces.value(kGattCharIface).value(QStringLiteral("UUID")).toString();
        if (uuid.compare(kUuidRx, Qt::CaseInsensitive) == 0)
            m_rxPath = path;
        else if (uuid.compare(kUuidTx, Qt::CaseInsensitive) == 0)
            m_txPath = path;
    }
    return !m_rxPath.isEmpty() && !m_txPath.isEmpty();
}

void BleTransport::open()
{
    if (m_ready)
        return;

    if (!findDevice()) {
        fail(tr("Flipper not found. Pair it in Settings → Bluetooth first."));
        return;
    }

    m_adapterPath = m_devicePath.left(m_devicePath.lastIndexOf(QLatin1Char('/')));

    QDBusConnection bus = QDBusConnection::systemBus();
    // Watch device (ServicesResolved / Connected) and the TX char (Value).
    bus.connect(kBluez, m_devicePath, kPropsIface, QStringLiteral("PropertiesChanged"),
                this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    QDBusInterface dev(kBluez, m_devicePath, kDeviceIface, bus);

    // Always start from a CLEAN link. Reusing an existing BLE connection leaves
    // the Flipper's RPC session stuck: the app looks "connected" but no
    // responses ever arrive. If a link is up (e.g. left over from a previous
    // app session that wasn't torn down), drop it first — this resets the
    // Flipper's BLE RPC, exactly like toggling Bluetooth does — then reconnect.
    if (dev.property("Connected").toBool()) {
        dev.call(QStringLiteral("Disconnect"));   // synchronous
    }

    // A bare Connect() often can't find a bonded LE device unless BlueZ is
    // actively scanning (which is why pairing via Settings works). Run LE
    // discovery first, then connect; tryStartNotify() fires on ServicesResolved.
    startDiscovery();
    QTimer::singleShot(2500, this, [this]() { connectDevice(); });

    // Watchdog: surface a clear error if the link never comes up.
    QTimer::singleShot(20000, this, [this]() {
        if (!m_ready)
            fail(tr("BLE timed out. Make sure the Flipper is awake (home screen) "
                    "with Bluetooth on, then try again."));
    });
}

void BleTransport::startDiscovery()
{
    if (m_discovering || m_adapterPath.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface adapter(kBluez, m_adapterPath, QStringLiteral("org.bluez.Adapter1"), bus);
    QVariantMap filter;
    filter.insert(QStringLiteral("Transport"), QStringLiteral("le"));
    adapter.call(QStringLiteral("SetDiscoveryFilter"), QVariant::fromValue(filter));
    adapter.call(QStringLiteral("StartDiscovery"));
    m_discovering = true;
}

void BleTransport::stopDiscovery()
{
    if (!m_discovering || m_adapterPath.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface adapter(kBluez, m_adapterPath, QStringLiteral("org.bluez.Adapter1"), bus);
    adapter.asyncCall(QStringLiteral("StopDiscovery"));
    m_discovering = false;
}

void BleTransport::connectDevice()
{
    if (m_ready)
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface dev(kBluez, m_devicePath, kDeviceIface, bus);
    QDBusPendingCall pc = dev.asyncCall(QStringLiteral("Connect"));
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(pc, this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<> reply = *self;
        if (reply.isError() && !m_ready)
            qWarning() << "harbour-sflipper: Connect:" << reply.error().message();
        self->deleteLater();
    });
}

void BleTransport::tryStartNotify()
{
    if (m_notifying)
        return;

    stopDiscovery();
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(kBluez, m_txPath, kPropsIface, QStringLiteral("PropertiesChanged"),
                this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    m_rxIface = new QDBusInterface(kBluez, m_rxPath, kGattCharIface, bus, this);

    // Best-effort larger write chunk if BlueZ exposes the negotiated MTU.
    const QVariant mtu = m_rxIface->property("MTU");
    if (mtu.isValid() && mtu.toInt() > 23)
        m_mtu = mtu.toInt() - 3;

    // BlueZ delivers the Value PropertiesChanged only to clients that hold their
    // OWN notify session, so we must always StartNotify on this connection.
    // Best-effort: log but don't fail the transport if it errors.
    QDBusInterface tx(kBluez, m_txPath, kGattCharIface, bus);
    QDBusReply<void> r = tx.call(QStringLiteral("StartNotify"));
    if (!r.isValid())
        qWarning() << "harbour-sflipper: StartNotify failed:" << r.error().message();

    m_notifying = true;
    m_ready = true;
    emit ready();
}

void BleTransport::onPropertiesChanged(const QString &interface,
                                       const QVariantMap &changed,
                                       const QStringList &invalidated)
{
    Q_UNUSED(invalidated)

    if (interface == kDeviceIface) {
        if (changed.value(QStringLiteral("ServicesResolved")).toBool())
            tryStartNotify();
        if (changed.contains(QStringLiteral("Connected"))
            && !changed.value(QStringLiteral("Connected")).toBool() && m_ready) {
            m_ready = false;
            m_notifying = false;
            emit closed();
        }
        return;
    }

    if (interface == kGattCharIface && changed.contains(QStringLiteral("Value"))) {
        const QVariant v = changed.value(QStringLiteral("Value"));
        QByteArray bytes;
        if (v.canConvert<QByteArray>()) {
            bytes = v.toByteArray();
        } else if (v.canConvert<QDBusArgument>()) {
            QDBusArgument a = v.value<QDBusArgument>();
            a >> bytes;
        }
        if (!bytes.isEmpty())
            emit bytesReceived(bytes);
    }
}

void BleTransport::write(const QByteArray &data)
{
    if (!m_ready || !m_rxIface)
        return;

    QVariantMap options;
    options.insert(QStringLiteral("type"), QStringLiteral("command")); // write-without-response

    for (int off = 0; off < data.size(); off += m_mtu) {
        const QByteArray chunk = data.mid(off, m_mtu);
        m_rxIface->asyncCall(QStringLiteral("WriteValue"),
                             QVariant::fromValue(chunk),
                             QVariant::fromValue(options));
    }
}

void BleTransport::close()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!m_txPath.isEmpty()) {
        bus.disconnect(kBluez, m_txPath, kPropsIface, QStringLiteral("PropertiesChanged"),
                       this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
        if (m_notifying) {
            QDBusInterface tx(kBluez, m_txPath, kGattCharIface, bus);
            tx.asyncCall(QStringLiteral("StopNotify"));
        }
    }
    stopDiscovery();
    if (!m_devicePath.isEmpty()) {
        bus.disconnect(kBluez, m_devicePath, kPropsIface, QStringLiteral("PropertiesChanged"),
                       this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
        // Drop the BLE link so the Flipper tears down its RPC session — leaving
        // it up is what makes the next connect "connected but silent".
        QDBusInterface dev(kBluez, m_devicePath, kDeviceIface, bus);
        dev.asyncCall(QStringLiteral("Disconnect"));
    }
    if (m_rxIface) {
        m_rxIface->deleteLater();
        m_rxIface = nullptr;
    }
    const bool was = m_ready;
    m_ready = false;
    m_notifying = false;
    if (was)
        emit closed();
}
