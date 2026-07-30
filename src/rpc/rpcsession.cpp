#include "rpcsession.h"
#include "../transport/transport.h"

#include <QString>
#include <QDebug>
#include <string.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include "flipper.pb.h"

// ---------------------------------------------------------------------------
// Encoding helpers
// ---------------------------------------------------------------------------
static QByteArray encodeDelimited(const PB_Main *main)
{
    size_t sz = 0;
    if (!pb_get_encoded_size(&sz, PB_Main_fields, main))
        return QByteArray();
    QByteArray out;
    out.resize(int(sz) + 8);            // room for the varint length prefix
    pb_ostream_t stream = pb_ostream_from_buffer(
        reinterpret_cast<pb_byte_t *>(out.data()), out.size());
    if (!pb_encode_ex(&stream, PB_Main_fields, main, PB_ENCODE_DELIMITED))
        return QByteArray();
    out.resize(int(stream.bytes_written));
    return out;
}

RpcSession::RpcSession(QObject *parent) : QObject(parent) {}

void RpcSession::attach(Transport *transport)
{
    if (m_transport)
        disconnect(m_transport, nullptr, this, nullptr);
    m_transport = transport;
    if (m_transport)
        connect(m_transport, &Transport::bytesReceived, this, &RpcSession::onBytes);
    reset();
}

void RpcSession::reset()
{
    m_rx.clear();
}

quint32 RpcSession::sendMain(void *mainPtr)
{
    if (!m_transport || !m_transport->isOpen())
        return 0;
    PB_Main *main = static_cast<PB_Main *>(mainPtr);
    QByteArray frame = encodeDelimited(main);
    if (frame.isEmpty()) {
        emit protocolError(tr("Failed to encode RPC request"));
        return 0;
    }
    m_transport->write(frame);
    return main->command_id;
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------
quint32 RpcSession::requestDeviceInfo()
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_system_device_info_request_tag;
    return sendMain(&main);
}

quint32 RpcSession::requestPowerInfo()
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_system_power_info_request_tag;
    return sendMain(&main);
}

quint32 RpcSession::startScreenStream()
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_gui_start_screen_stream_request_tag;
    return sendMain(&main);
}

quint32 RpcSession::stopScreenStream()
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_gui_stop_screen_stream_request_tag;
    return sendMain(&main);
}

quint32 RpcSession::sendInput(int key, int type)
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_gui_send_input_event_request_tag;
    main.content.gui_send_input_event_request.key = PB_Gui_InputKey(key);
    main.content.gui_send_input_event_request.type = PB_Gui_InputType(type);
    return sendMain(&main);
}

quint32 RpcSession::storageList(const QString &path)
{
    QByteArray p = path.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_list_request_tag;
    main.content.storage_list_request.path = p.data();
    return sendMain(&main);
}

quint32 RpcSession::storageRead(const QString &path)
{
    QByteArray p = path.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_read_request_tag;
    main.content.storage_read_request.path = p.data();
    return sendMain(&main);
}

quint32 RpcSession::storageWrite(const QString &path, const QByteArray &data)
{
    QByteArray p = path.toUtf8();
    QByteArray arrBuf;
    arrBuf.resize(int(PB_BYTES_ARRAY_T_ALLOCSIZE(size_t(data.size()))));
    pb_bytes_array_t *arr = reinterpret_cast<pb_bytes_array_t *>(arrBuf.data());
    arr->size = data.size();
    if (data.size() > 0)
        memcpy(arr->bytes, data.constData(), data.size());

    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_write_request_tag;
    main.content.storage_write_request.path = p.data();
    main.content.storage_write_request.has_file = true;
    main.content.storage_write_request.file.data = arr;
    return sendMain(&main);
}

quint32 RpcSession::allocCommandId()
{
    return nextId();
}

void RpcSession::storageWriteChunk(quint32 commandId, const QString &path,
                                   const QByteArray &chunk, bool hasNext)
{
    QByteArray p = path.toUtf8();
    QByteArray arrBuf;
    arrBuf.resize(int(PB_BYTES_ARRAY_T_ALLOCSIZE(size_t(chunk.size()))));
    pb_bytes_array_t *arr = reinterpret_cast<pb_bytes_array_t *>(arrBuf.data());
    arr->size = chunk.size();
    if (chunk.size() > 0)
        memcpy(arr->bytes, chunk.constData(), chunk.size());

    PB_Main main = PB_Main_init_zero;
    main.command_id = commandId;
    main.has_next = hasNext;
    main.which_content = PB_Main_storage_write_request_tag;
    main.content.storage_write_request.path = p.data();
    main.content.storage_write_request.has_file = true;
    main.content.storage_write_request.file.data = arr;
    sendMain(&main);
}

quint32 RpcSession::storageDelete(const QString &path, bool recursive)
{
    QByteArray p = path.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_delete_request_tag;
    main.content.storage_delete_request.path = p.data();
    main.content.storage_delete_request.recursive = recursive;
    return sendMain(&main);
}

quint32 RpcSession::storageMkdir(const QString &path)
{
    QByteArray p = path.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_mkdir_request_tag;
    main.content.storage_mkdir_request.path = p.data();
    return sendMain(&main);
}

quint32 RpcSession::storageRename(const QString &oldPath, const QString &newPath)
{
    QByteArray o = oldPath.toUtf8();
    QByteArray n = newPath.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_storage_rename_request_tag;
    main.content.storage_rename_request.old_path = o.data();
    main.content.storage_rename_request.new_path = n.data();
    return sendMain(&main);
}

quint32 RpcSession::appStart(const QString &name, const QString &args)
{
    QByteArray n = name.toUtf8();
    QByteArray a = args.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_app_start_request_tag;
    main.content.app_start_request.name = n.data();
    main.content.app_start_request.args = a.data();
    return sendMain(&main);
}

quint32 RpcSession::systemReboot(int mode)
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_system_reboot_request_tag;
    main.content.system_reboot_request.mode = PB_System_RebootRequest_RebootMode(mode);
    return sendMain(&main);
}

quint32 RpcSession::systemUpdate(const QString &manifestPath)
{
    QByteArray p = manifestPath.toUtf8();
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_system_update_request_tag;
    main.content.system_update_request.update_manifest = p.data();
    return sendMain(&main);
}

quint32 RpcSession::ping()
{
    PB_Main main = PB_Main_init_zero;
    main.command_id = nextId();
    main.which_content = PB_Main_system_ping_request_tag;
    return sendMain(&main);
}

// ---------------------------------------------------------------------------
// Decoding
// ---------------------------------------------------------------------------
void RpcSession::onBytes(const QByteArray &data)
{
    m_rx += data;
    decodeBuffer();
}

void RpcSession::decodeBuffer()
{
    for (;;) {
        // Read the varint length prefix without consuming until we know the
        // whole frame is present.
        quint32 len = 0;
        int shift = 0;
        int pos = 0;
        bool complete = false;
        while (pos < m_rx.size()) {
            quint8 b = quint8(m_rx.at(pos));
            len |= quint32(b & 0x7f) << shift;
            pos++;
            if (!(b & 0x80)) { complete = true; break; }
            shift += 7;
            if (shift > 35) {
                emit protocolError(tr("Malformed RPC length prefix"));
                m_rx.clear();
                return;
            }
        }
        if (!complete)
            return;                              // wait for more bytes
        if (m_rx.size() - pos < int(len))
            return;                              // frame not fully arrived

        QByteArray frame = m_rx.mid(pos, int(len));
        m_rx.remove(0, pos + int(len));
        handleMain(frame);
    }
}

void RpcSession::handleMain(const QByteArray &frame)
{
    PB_Main main = PB_Main_init_zero;
    pb_istream_t s = pb_istream_from_buffer(
        reinterpret_cast<const pb_byte_t *>(frame.constData()), frame.size());
    if (!pb_decode(&s, PB_Main_fields, &main)) {
        emit protocolError(tr("RPC decode error"));
        pb_release(PB_Main_fields, &main);
        return;
    }

    const quint32 cmdId = main.command_id;
    const bool hasNext = main.has_next;
    const int status = int(main.command_status);

    // NB: do NOT surface a non-OK command_status as a protocol error here.
    // Command-level results are delivered to their specific handlers
    // (commandComplete / updateResult), which decide what is fatal. Many
    // non-OK statuses are benign — e.g. mkdir on an existing directory returns
    // ERROR_STORAGE_EXIST (6) during an app/firmware upload. Just log it.
    if (status != PB_CommandStatus_OK)
        qWarning() << "RpcSession: command" << cmdId << "status" << status;

    switch (main.which_content) {
    case PB_Main_system_device_info_response_tag: {
        const char *k = main.content.system_device_info_response.key;
        const char *v = main.content.system_device_info_response.value;
        emit deviceInfoPair(QString::fromUtf8(k ? k : ""), QString::fromUtf8(v ? v : ""));
        if (!hasNext)
            emit deviceInfoComplete();
        break;
    }
    case PB_Main_system_power_info_response_tag: {
        const char *k = main.content.system_power_info_response.key;
        const char *v = main.content.system_power_info_response.value;
        emit powerInfoPair(QString::fromUtf8(k ? k : ""), QString::fromUtf8(v ? v : ""));
        if (!hasNext)
            emit powerInfoComplete();
        break;
    }
    case PB_Main_gui_screen_frame_tag: {
        const pb_bytes_array_t *d = main.content.gui_screen_frame.data;
        QByteArray bytes;
        if (d)
            bytes = QByteArray(reinterpret_cast<const char *>(d->bytes), int(d->size));
        emit screenFrame(bytes, int(main.content.gui_screen_frame.orientation));
        break;
    }
    case PB_Main_storage_list_response_tag: {
        const PB_Storage_ListResponse &lr = main.content.storage_list_response;
        for (pb_size_t i = 0; i < lr.file_count; ++i) {
            const PB_Storage_File &f = lr.file[i];
            const bool isDir = (f.type == PB_Storage_File_FileType_DIR);
            emit storageListEntry(cmdId, QString::fromUtf8(f.name ? f.name : ""),
                                  isDir, f.size);
        }
        if (!hasNext)
            emit storageListComplete(cmdId);
        break;
    }
    case PB_Main_storage_read_response_tag: {
        const PB_Storage_ReadResponse &rr = main.content.storage_read_response;
        QByteArray bytes;
        if (rr.has_file && rr.file.data)
            bytes = QByteArray(reinterpret_cast<const char *>(rr.file.data->bytes),
                               int(rr.file.data->size));
        emit storageReadData(cmdId, bytes, hasNext);
        break;
    }
    case PB_Main_system_update_response_tag:
        emit updateResult(cmdId, int(main.content.system_update_response.code));
        break;
    case PB_Main_empty_tag:
    default:
        if (!hasNext)
            emit commandComplete(cmdId, status);
        break;
    }

    pb_release(PB_Main_fields, &main);
}
