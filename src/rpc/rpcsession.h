#ifndef FLIPPER_RPCSESSION_H
#define FLIPPER_RPCSESSION_H

#include <QObject>
#include <QByteArray>

class Transport;

// Transport-agnostic Flipper RPC layer. Encodes PB_Main requests (varint
// length-delimited protobuf) and decodes the streamed responses, correlating
// them by command_id and reassembling multi-frame (has_next) replies.
//
// Framing validated live against /dev/ttyACM0: e.g. a device-info reply frame
// begins `1d 08 01 18 01 8a 02 …` = len(29) command_id(1) has_next(1) field33…
class RpcSession : public QObject
{
    Q_OBJECT
public:
    explicit RpcSession(QObject *parent = nullptr);

    // Wire a transport in/out. Does not take ownership.
    void attach(Transport *transport);

    // Input keys/types mirror PB_Gui_InputKey / PB_Gui_InputType.
    enum InputKey { Up = 0, Down = 1, Right = 2, Left = 3, Ok = 4, Back = 5 };
    enum InputType { Press = 0, Release = 1, Short = 2, Long = 3, Repeat = 4 };

    // Requests — each returns the command_id used (0 if not connected).
    quint32 requestDeviceInfo();
    quint32 requestPowerInfo();
    quint32 startScreenStream();
    quint32 stopScreenStream();
    quint32 sendInput(int key, int type);
    quint32 storageList(const QString &path);
    quint32 storageRead(const QString &path);
    quint32 storageWrite(const QString &path, const QByteArray &data);
    quint32 allocCommandId();                // reserve a command_id for chunking
    void storageWriteChunk(quint32 commandId, const QString &path,
                           const QByteArray &chunk, bool hasNext);
    quint32 storageDelete(const QString &path, bool recursive);
    quint32 storageMkdir(const QString &path);
    quint32 storageRename(const QString &oldPath, const QString &newPath);
    quint32 appStart(const QString &name, const QString &args);
    quint32 systemReboot(int mode);          // 0=OS 1=DFU 2=UPDATE
    quint32 systemUpdate(const QString &manifestPath);   // stage an OTA update
    quint32 ping();

    void reset();                             // drop rx buffer / state

signals:
    void deviceInfoPair(const QString &key, const QString &value);
    void deviceInfoComplete();
    void powerInfoPair(const QString &key, const QString &value);
    void powerInfoComplete();
    void screenFrame(const QByteArray &data, int orientation);
    void storageListEntry(quint32 commandId, const QString &name, bool isDir, quint32 size);
    void storageListComplete(quint32 commandId);
    void storageReadData(quint32 commandId, const QByteArray &data, bool hasNext);
    void commandComplete(quint32 commandId, int status);   // empty/ack responses
    void updateResult(quint32 commandId, int code);        // system_update_response
    void protocolError(const QString &message);

private slots:
    void onBytes(const QByteArray &data);

private:
    quint32 nextId() { return ++m_commandId; }
    quint32 sendMain(void *mainPtr);          // encode+write, returns command_id
    void decodeBuffer();
    void handleMain(const QByteArray &frame);

    Transport *m_transport = nullptr;
    QByteArray m_rx;
    quint32 m_commandId = 0;
};

#endif // FLIPPER_RPCSESSION_H
