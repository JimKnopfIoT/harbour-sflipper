#ifndef FLIPPER_SERIALTRANSPORT_H
#define FLIPPER_SERIALTRANSPORT_H

#include "transport.h"

class QSocketNotifier;

// USB-C transport over a CDC-ACM tty (default /dev/ttyACM0).
//
// The Flipper boots its serial link into the text CLI. We send
// "start_rpc_session\r" to switch it into binary RPC mode, swallow the echo,
// and only then emit ready() — after which bytesReceived() carries raw RPC
// frames. No QtSerialPort dependency: raw POSIX termios + QSocketNotifier,
// matching how the device was validated live over /dev/ttyACM0.
class SerialTransport : public Transport
{
    Q_OBJECT
public:
    explicit SerialTransport(const QString &device = QStringLiteral("/dev/ttyACM0"),
                             QObject *parent = nullptr);
    ~SerialTransport() override;

    void open() override;
    void close() override;
    bool isOpen() const override { return m_fd >= 0; }
    void write(const QByteArray &data) override;
    QString name() const override { return QStringLiteral("USB-C"); }

private slots:
    void onReadable();

private:
    void rawWrite(const QByteArray &data);

    QString m_device;
    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    bool m_rpcReady = false;   // true once the CLI→RPC handshake is done
    QByteArray m_handshakeEcho;
};

#endif // FLIPPER_SERIALTRANSPORT_H
