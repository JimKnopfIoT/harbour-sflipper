# Mouse-Jiggler-smart (Flipper application)

This is the **source** of the small Flipper Zero application that sFlipper can
install to the device (see the app's *USB device library* screen). A prebuilt
copy is bundled with the phone package at
`/usr/share/harbour-sflipper/apps/mouse_jiggler_smart.fap`; this directory is its
corresponding source (GPL-3.0-or-later, like the rest of the project).

The app presents a chosen USB device identity — VID/PID plus manufacturer and
product strings — so the Flipper enumerates on a target PC as a saved mouse
instead of the generic Flipper HID device, then does a tiny stealth jiggle at a
random interval. The identity is read from
`/ext/apps_data/mouse_jiggler_smart/id.conf` (key=value, `vid`/`pid` in hex),
which the sFlipper phone app writes when you push a profile.

## Building

It builds against the Flipper firmware as a user application (`.fap`). It targets
the **Unleashed** firmware but uses only stock fap-API symbols, so official
firmware works too.

With [`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt) (standalone):

```sh
ufbt          # from inside this directory
```

Or inside a firmware checkout — drop this directory into `applications_user/`
and build from the firmware root:

```sh
./fbt COMPACT=1 DEBUG=0 fap_mouse_jiggler_smart
```

The resulting `mouse_jiggler_smart.fap` (under `build/.../.extapps/`) is what is
bundled in the phone package. Copy a freshly built one over
`data/apps/mouse_jiggler_smart.fap` to update the bundled binary.
