# Attribution

harbour-sflipper is an **unofficial**, community-built SailfishOS client for the
Flipper Zero. Not affiliated with or endorsed by Flipper Devices Inc. "Flipper
Zero" and the dolphin mascot are trademarks of their respective owners.

Bundled / derived third-party work:

- **App icon** — dolphin line-art from [Arcticons](https://github.com/Arcticons-Team/Arcticons),
  licensed Apache-2.0, recoloured onto an orange tile. See `icons/icon.svg`.
- **nanopb** (`lib/nanopb/`) — protobuf runtime for C, zlib license, © Petteri Aimonen.
- **flipperzero-protobuf** (`lib/flipperzero-protobuf/`) — RPC `.proto` definitions
  by Flipper Devices, used to generate `lib/generated/`. See that directory's LICENSE.
- Protocol details cross-checked against the open-source **qFlipper** (GPL-3.0) and
  the Flipper firmware; no source code was copied.

The bundled Flipper application `data/apps/mouse_jiggler_smart.fap` is **first-party**
code (GPL-3.0-or-later); its corresponding source is in
`flipper-app/mouse_jiggler_smart/`. It builds against the Flipper firmware
(fap API) and links no third-party code beyond that public API.
