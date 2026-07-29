# harbour-sflipper — Protokoll- & Architektur-Referenz

Native SailfishOS-App für den **Flipper Zero**. Ziel: Funktionsparität mit der
offiziellen Android-App (`com.flipperdevices.app`), aber ohne Android App Support
(Akku!) — echtes Silica/Qt, BLE **und** USB-C.

## Verbindung / Transport

Flippers RPC-Protokoll (Protobuf) ist über BLE und USB **identisch**. Deshalb:
`Transport` (abstrakt) → zwei Backends, darüber der unveränderte RPC-Layer.

### A) BLE (BlueZ D-Bus GATT, wie bei harbour-idrone/LabCam-Ansatz)

Serial Service + Characteristics (Firmware `serial_service_uuid.inc`, Bytes dort
little-endian; hier als kanonische UUID):

| Rolle        | UUID                                   | Properties        | Richtung        |
|--------------|----------------------------------------|-------------------|-----------------|
| Service      | `8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000` | primary           | —               |
| **TX**       | `19ed82ae-ed21-4c9d-4145-228e61fe0000` | notify            | Flipper → Phone |
| **RX**       | `19ed82ae-ed21-4c9d-4145-228e62fe0000` | write             | Phone → Flipper |
| Flow-Control | `19ed82ae-ed21-4c9d-4145-228e63fe0000` | notify            | Flipper → Phone |
| RPC-Status   | `19ed82ae-ed21-4c9d-4145-228e64fe0000` | notify            | Flipper → Phone |

- Zentrale (Phone): **subscribe** auf TX (StartNotify), **write** auf RX.
- RX in MTU-große Chunks stückeln; Flow-Control-Notify liefert den freien
  Puffer (uint32 LE) des Flipper — vor dem nächsten Write beachten, sonst Overflow.
- RPC-Status-Char signalisiert, ob die RPC-Session aktiv ist.
- Über BLE ist die Verbindung **direkt** im RPC-Modus (kein CLI-Handshake).

BlueZ-D-Bus-Pfad: `org.bluez` → Adapter `/org/bluez/hci0`, Device
`/org/bluez/hci0/dev_XX_XX_...`, `Device1.Connect()`, dann
`GattCharacteristic1.{WriteValue,StartNotify,AcquireWrite/AcquireNotify}`.
`AcquireNotify`/`AcquireWrite` (Datei-Deskriptor) ist performanter als
`WriteValue`/PropertiesChanged für den Stream — für Screen-Streaming wichtig.

### B) USB-C (CDC-ACM)

Flipper meldet sich am USB-OTG als `/dev/ttyACM0` (CDC serial). Ablauf:
1. Port öffnen (115200 8N1, raw termios).
2. CLI ist aktiv → **`start_rpc_session\r`** senden, um in den RPC-Modus zu wechseln.
3. Ab da identisches Protobuf-Framing wie über BLE.

## RPC-Framing

Ein RPC-Kommando ist eine `PB_Main`-Message (`flipper.proto`), **varint-längen-
delimitiert** übertragen: `varint(len) || PB_Main-bytes`. Genauso wie nanopb
`pb_encode_ex(..., PB_ENCODE_DELIMITED)` / `pb_decode_ex(..., PB_DECODE_DELIMITED)`.

`PB_Main` (flipper.proto):
- `uint32 command_id` — korreliert Request↔Response (frei wählbar, Echo in Antwort).
- `CommandStatus command_status` — Fehlercode in Antworten.
- `bool has_next` — 1 = weitere Fragmente derselben logischen Antwort folgen
  (z. B. lange Verzeichnislisten, Screen-Frames). Erst zusammensetzen, wenn
  `has_next == false`.
- `oneof content` — die eigentliche Nachricht, u. a.:
  - System: `system_ping_request/response`, `system_device_info_request` (→ ge-
    streamte key/value-Paare), `system_power_info_request`, `system_reboot_request`.
  - Storage: `storage_list_request`, `storage_read_request`, `storage_write_request`,
    `storage_delete_request`, `storage_mkdir_request`, `storage_md5sum_request`,
    `storage_rename_request`, `storage_info_request`, `storage_stat_request`.
  - Gui: `gui_start_screen_stream_request` (→ `gui_screen_frame` Notifies, 1024 B =
    128×64 1bpp), `gui_send_input_event_request` (Key Up/Down/Left/Right/Ok/Back,
    Type Press/Release/Short/Long), `gui_start_virtual_display_request`.
  - App: `app_start_request` (App + Args, z. B. `"Sub-GHz"`/Pfad einer `.sub`),
    `app_lock_status_request`, `app_exit_request`, `app_load_file_request`,
    `app_button_press_request`.
  - Gpio, Property, Desktop analog.

## Feature-Map (Android-Parität → RPC)

| App-Feature              | RPC                                                        | Meilenstein |
|--------------------------|-----------------------------------------------------------|-------------|
| Geräteinfo/Akku          | `system_device_info`, `system_power_info`                 | M2          |
| Bildschirm spiegeln      | `gui_start_screen_stream` → `gui_screen_frame`            | M3          |
| Fernsteuerung (Tasten)   | `gui_send_input_event`                                     | M3          |
| Dateimanager             | `storage_*`                                               | M4          |
| Gespeichertes Signal     | `app_start` (`Sub-GHz`/`Infrared`/`NFC`/`RFID` + Pfad)    | M5          |
| Firmware-/Radio-Update   | `storage_write` (Bundle) + `system_update`/DFU            | M6+         |
| GPIO                     | `gpio_*`                                                  | M6+         |

## Quellen (vendored unter `lib/`)

- `lib/flipperzero-protobuf/` — offizielle `.proto` (LGPL/MIT, flipperdevices).
- `lib/nanopb/` — Protobuf-Encoder/Decoder für C (zlib-Lizenz).
- Referenz-Implementierung Desktop: qFlipper (GPLv3) — nur als Protokoll-Vorlage,
  kein Code übernommen.
