#ifndef FLIPPER_TRANSPORT_H
#define FLIPPER_TRANSPORT_H

#include <QObject>
#include <QByteArray>

// Abstract byte-stream link to a Flipper Zero. Two backends implement it:
//   SerialTransport — USB-C CDC-ACM (/dev/ttyACM0), CLI→RPC handshake
//   BleTransport    — BLE via BlueZ D-Bus GATT (Serial service)
// Above this sits RpcSession, which is transport-agnostic: the Flipper RPC
// (protobuf) framing is identical over USB and BLE.
class Transport : public QObject
{
    Q_OBJECT
public:
    explicit Transport(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~Transport() {}

    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Raw RPC bytes (already past any CLI handshake).
    virtual void write(const QByteArray &data) = 0;

    virtual QString name() const = 0;   // e.g. "USB-C" / "BLE"

signals:
    void ready();                       // link up, RPC session may begin
    void closed();
    void bytesReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
};

#endif // FLIPPER_TRANSPORT_H
