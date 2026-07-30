// Mouse-Jiggler-smart — a stealth USB mouse jiggler that presents a chosen USB
// device identity (VID/PID + manufacturer/product strings). The identity is
// written by the sFlipper phone app to:
//     /ext/apps_data/mouse_jiggler_smart/id.conf   (key=value, vid/pid in hex)
// so the Flipper enumerates on a target PC as that saved mouse instead of the
// generic Flipper HID device.

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>

// Read the profile from the absolute path the sFlipper phone app writes to.
// (We deliberately do NOT use APP_DATA_PATH / the "/data" alias here — that
// alias did not resolve to this file at runtime; the absolute path is exactly
// what the phone writes and what the file browser shows.)
#define CONF_PATH EXT_PATH("apps_data/mouse_jiggler_smart/id.conf")
#define TAG "MouseJigglerSmart"

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    FuriTimer* timer;
    FuriMutex* mutex;

    // Loaded identity.
    FuriHalUsbHidConfig hid_cfg;
    char name[64];
    bool have_profile;
    char status[48]; // diagnostic: config open/read result

    // Jiggle state.
    bool running;
    bool connected;
    int min_min; // random interval lower bound (minutes)
    int max_min; // random interval upper bound (minutes)
} JigglerApp;

typedef struct {
    InputEvent input;
} JigglerEvent;

// --- config parsing --------------------------------------------------------

static void trim(char* s) {
    // strip trailing CR/LF/space
    size_t n = strlen(s);
    while(n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
        s[--n] = '\0';
    }
}

static void jiggler_load_config(JigglerApp* app) {
    // Defaults: a common Logitech identity, so the app is usable even without a
    // pushed profile.
    memset(&app->hid_cfg, 0, sizeof(app->hid_cfg));
    app->hid_cfg.vid = HID_VID_DEFAULT;
    app->hid_cfg.pid = HID_PID_DEFAULT;
    strlcpy(app->name, "Default (no profile)", sizeof(app->name));
    app->have_profile = false;

    strlcpy(app->status, "conf: not opened", sizeof(app->status));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    do {
        if(!storage_file_open(file, CONF_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            strlcpy(app->status, "conf: OPEN FAILED", sizeof(app->status));
            break;
        }

        uint64_t size = storage_file_size(file);
        if(size == 0 || size > 4096) size = 4096;
        char* buf = malloc(size + 1);
        size_t rd = storage_file_read(file, buf, size);
        buf[rd] = '\0';
        snprintf(app->status, sizeof(app->status), "conf: read %u B", (unsigned)rd);

        app->have_profile = true;
        bool got_name = false;

        // Parse line by line: key=value. (strtok_r is not in the fap API, so we
        // split on '\n' by hand.)
        char* line = buf;
        while(line != NULL && *line != '\0') {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';

            char* eq = strchr(line, '=');
            if(eq == NULL) {
                line = nl ? nl + 1 : NULL;
                continue;
            }
            *eq = '\0';
            char* key = line;
            char* val = eq + 1;
            trim(key);
            trim(val);

            if(strcmp(key, "vid") == 0) {
                app->hid_cfg.vid = (uint16_t)strtol(val, NULL, 16);
            } else if(strcmp(key, "pid") == 0) {
                app->hid_cfg.pid = (uint16_t)strtol(val, NULL, 16);
            } else if(strcmp(key, "manuf") == 0) {
                strlcpy(app->hid_cfg.manuf, val, HID_MANUF_PRODUCT_NAME_LEN);
            } else if(strcmp(key, "product") == 0) {
                strlcpy(app->hid_cfg.product, val, HID_MANUF_PRODUCT_NAME_LEN);
            } else if(strcmp(key, "name") == 0) {
                strlcpy(app->name, val, sizeof(app->name));
                got_name = true;
            }

            line = nl ? nl + 1 : NULL; // advance to the next line (was missing!)
        }
        if(!got_name && app->hid_cfg.product[0]) {
            strlcpy(app->name, app->hid_cfg.product, sizeof(app->name));
        }
        free(buf);
    } while(false);

    // Guard against a zeroed identity that USB would reject.
    if(app->hid_cfg.vid == 0) app->hid_cfg.vid = HID_VID_DEFAULT;
    if(app->hid_cfg.pid == 0) app->hid_cfg.pid = HID_PID_DEFAULT;

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- rendering -------------------------------------------------------------

static void jiggler_draw_callback(Canvas* canvas, void* ctx) {
    JigglerApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Mouse Jiggler smart");

    canvas_set_font(canvas, FontSecondary);
    char line[72];

    // Identity name (truncated by canvas).
    canvas_draw_str(canvas, 2, 23, app->name);

    snprintf(
        line, sizeof(line), "USB %04lX:%04lX", (unsigned long)app->hid_cfg.vid,
        (unsigned long)app->hid_cfg.pid);
    canvas_draw_str(canvas, 2, 33, line);

    snprintf(line, sizeof(line), "Interval %d-%d min", app->min_min, app->max_min);
    canvas_draw_str(canvas, 2, 43, line);

    // Diagnostic line (config open/read result) — only when no profile loaded.
    if(!app->have_profile) {
        canvas_draw_str(canvas, 2, 53, app->status);
    }

    if(app->running) {
        snprintf(line, sizeof(line), "RUNNING  %s", app->connected ? "(host ok)" : "(no host)");
    } else {
        strlcpy(line, "OK: Start   Back: Quit", sizeof(line));
    }
    canvas_draw_str(canvas, 2, 63, line);

    furi_mutex_release(app->mutex);
}

static void jiggler_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    JigglerEvent event = {.input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

// --- jiggle timer ----------------------------------------------------------

static uint32_t jiggler_random_interval_ms(JigglerApp* app) {
    int span = app->max_min - app->min_min;
    int minutes = app->min_min + (span > 0 ? (rand() % (span + 1)) : 0);
    if(minutes < 1) minutes = 1;
    return (uint32_t)minutes * 60000u;
}

static void jiggler_timer_callback(void* ctx) {
    JigglerApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool running = app->running;
    furi_mutex_release(app->mutex);

    if(!running) return;

    // Tiny stealth wiggle: move a couple pixels and immediately back, so the
    // cursor registers activity without visibly drifting.
    int8_t dx = (int8_t)((rand() % 5) - 2);
    int8_t dy = (int8_t)((rand() % 5) - 2);
    if(dx == 0 && dy == 0) dx = 1;
    bool ok = furi_hal_hid_mouse_move(dx, dy);
    furi_delay_ms(40);
    furi_hal_hid_mouse_move(-dx, -dy);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->connected = ok;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    furi_timer_start(app->timer, jiggler_random_interval_ms(app));
}

// --- entry point -----------------------------------------------------------

int32_t mouse_jiggler_smart_app(void* p) {
    UNUSED(p);

    JigglerApp* app = malloc(sizeof(JigglerApp));
    memset(app, 0, sizeof(JigglerApp));
    app->min_min = 1;
    app->max_min = 2;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(JigglerEvent));

    srand(furi_get_tick());
    jiggler_load_config(app);

    // Remember the current USB mode (usually CDC/RPC), but do NOT switch yet:
    // opening the app must not hijack USB. We only present our HID identity when
    // the user presses OK to start jiggling, and restore CDC on stop/exit — so
    // the phone can still write id.conf over RPC while this screen is open.
    FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
    bool usb_switched = false;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, jiggler_draw_callback, app);
    view_port_input_callback_set(app->view_port, jiggler_input_callback, app->queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    app->timer = furi_timer_alloc(jiggler_timer_callback, FuriTimerTypeOnce, app);

    JigglerEvent event;
    bool running_loop = true;
    while(running_loop) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        if(event.input.type == InputTypeLong && event.input.key == InputKeyBack) {
            break;
        }

        const bool press =
            (event.input.type == InputTypeShort || event.input.type == InputTypePress);

        // OK toggles jiggling. Starting takes over USB with our identity (after
        // re-reading the latest profile); stopping restores the previous mode.
        // Handle it outside the model mutex because config reload does file I/O.
        if(press && event.input.key == InputKeyOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool now_running = !app->running;
            app->running = now_running;
            furi_mutex_release(app->mutex);

            if(now_running) {
                jiggler_load_config(app); // pick up the latest pushed profile
                furi_hal_usb_unlock();
                usb_switched = furi_hal_usb_set_config(&usb_hid, &app->hid_cfg);
                // First jiggle after ~1.5 s so the host-connected status shows
                // right away; the timer callback then reschedules randomly.
                furi_timer_start(app->timer, 1500);
            } else {
                furi_timer_stop(app->timer);
                if(usb_switched) {
                    furi_hal_usb_set_config(usb_prev, NULL);
                    usb_switched = false;
                }
            }
            view_port_update(app->view_port);
            continue;
        }

        if(press) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            switch(event.input.key) {
            case InputKeyUp:
                if(!app->running && app->min_min < app->max_min) app->min_min++;
                break;
            case InputKeyDown:
                if(!app->running && app->min_min > 1) app->min_min--;
                break;
            case InputKeyRight:
                if(!app->running && app->max_min < 60) app->max_min++;
                break;
            case InputKeyLeft:
                if(!app->running && app->max_min > app->min_min + 1) app->max_min--;
                break;
            default:
                break;
            }
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }

    // Teardown; restore the previous USB mode only if we actually switched.
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    if(usb_switched) furi_hal_usb_set_config(usb_prev, NULL);

    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
