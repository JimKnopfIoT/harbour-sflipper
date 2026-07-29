#include "serialtransport.h"

#include <QSocketNotifier>
#include <QTimer>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

SerialTransport::SerialTransport(const QString &device, QObject *parent)
    : Transport(parent), m_device(device)
{
}

SerialTransport::~SerialTransport()
{
    close();
}

void SerialTransport::open()
{
    if (m_fd >= 0)
        return;

    m_fd = ::open(m_device.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        emit errorOccurred(tr("Cannot open %1: %2").arg(m_device, strerror(errno)));
        return;
    }

    // Raw 115200 8N1, no flow control — mirrors the validated live test.
    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(m_fd, &tio) != 0) {
        emit errorOccurred(tr("tcgetattr failed: %1").arg(strerror(errno)));
        close();
        return;
    }
    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tcsetattr(m_fd, TCSANOW, &tio);
    tcflush(m_fd, TCIOFLUSH);

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &SerialTransport::onReadable);

    // Kick the CLI, then switch it into binary RPC mode. The device echoes the
    // command line back; onReadable() swallows echo until the newline and only
    // then flips m_rpcReady and emits ready().
    m_rpcReady = false;
    m_handshakeEcho.clear();
    rawWrite(QByteArrayLiteral("\r\n"));
    QTimer::singleShot(150, this, [this]() {
        rawWrite(QByteArrayLiteral("start_rpc_session\r"));
    });
    // Fallback: if we never cleanly see the echo, go ready anyway shortly after.
    QTimer::singleShot(600, this, [this]() {
        if (m_fd >= 0 && !m_rpcReady) {
            m_rpcReady = true;
            m_handshakeEcho.clear();
            emit ready();
        }
    });
}

void SerialTransport::close()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
        m_rpcReady = false;
        emit closed();
    }
}

void SerialTransport::rawWrite(const QByteArray &data)
{
    if (m_fd < 0)
        return;
    qint64 off = 0;
    while (off < data.size()) {
        ssize_t n = ::write(m_fd, data.constData() + off, data.size() - off);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Buffer full → block until the Flipper drains it. This is the
                // natural back-pressure that paces large uploads over USB.
                struct pollfd pfd;
                pfd.fd = m_fd;
                pfd.events = POLLOUT;
                ::poll(&pfd, 1, 2000);
                continue;
            }
            emit errorOccurred(tr("write failed: %1").arg(strerror(errno)));
            return;
        }
        off += n;
    }
}

void SerialTransport::write(const QByteArray &data)
{
    rawWrite(data);
}

void SerialTransport::onReadable()
{
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(m_fd, buf, sizeof(buf));
        if (n > 0) {
            QByteArray chunk(buf, int(n));
            if (m_rpcReady) {
                emit bytesReceived(chunk);
            } else {
                // Swallow the CLI echo of "start_rpc_session\r". Once we see the
                // command echoed and its trailing newline, the stream that
                // follows is binary RPC.
                m_handshakeEcho += chunk;
                int nl = m_handshakeEcho.indexOf("start_rpc_session");
                if (nl >= 0) {
                    int eol = m_handshakeEcho.indexOf('\n', nl);
                    if (eol >= 0) {
                        QByteArray rest = m_handshakeEcho.mid(eol + 1);
                        m_rpcReady = true;
                        m_handshakeEcho.clear();
                        emit ready();
                        if (!rest.isEmpty())
                            emit bytesReceived(rest);
                    }
                }
            }
            continue;
        }
        if (n == 0)
            break;                       // no more data right now
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        if (errno == EINTR)
            continue;
        // Real error → link is gone (e.g. cable pulled).
        emit errorOccurred(tr("read failed: %1").arg(strerror(errno)));
        close();
        break;
    }
}
