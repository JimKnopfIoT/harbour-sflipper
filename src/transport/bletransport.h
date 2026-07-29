#ifndef FLIPPER_BLETRANSPORT_H
#define FLIPPER_BLETRANSPORT_H

#include "transport.h"
#include <QString>
#include <QByteArray>

class QDBusInterface;

// BLE transport over BlueZ D-Bus GATT (system bus, org.bluez).
//
// Pairing/bonding is done once through the Sailfish Bluetooth settings (the
// system agent handles the passkey shown on the Flipper). This transport then
// just locates the bonded Flipper, connects, subscribes to the Serial TX
// characteristic (indicate) and writes RPC frames to RX. Chars are matched by
// UUID, not object path.
//
//   Serial service   8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000
//   RX  (write)      19ed82ae-ed21-4c9d-4145-228e62fe0000   phone → flipper
//   TX  (indicate)   19ed82ae-ed21-4c9d-4145-228e61fe0000   flipper → phone
class BleTransport : public Transport
{
    Q_OBJECT
public:
    explicit BleTransport(QObject *parent = nullptr);
    ~BleTransport() override;

    void open() override;
    void close() override;
    bool isOpen() const override { return m_ready; }
    void write(const QByteArray &data) override;
    QString name() const override { return QStringLiteral("BLE"); }

private slots:
    void onPropertiesChanged(const QString &interface,
                             const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    bool findDevice();          // locate bonded Flipper + its RX/TX char paths
    void tryStartNotify();      // once services resolved
    void startDiscovery();      // help BlueZ (re)find the advertising Flipper
    void stopDiscovery();
    void connectDevice();
    void fail(const QString &msg);

    QString m_adapterPath;
    bool m_discovering = false;
    QString m_devicePath;
    QString m_rxPath;           // RX characteristic (write)
    QString m_txPath;           // TX characteristic (indicate)
    QDBusInterface *m_rxIface = nullptr;
    bool m_ready = false;
    bool m_notifying = false;
    int m_mtu = 20;             // conservative GATT write chunk
};

#endif // FLIPPER_BLETRANSPORT_H
