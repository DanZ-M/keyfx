/*
 * keyfx light engine
 *
 * A thin "led-strip" driver sits between ZMK's RGB underglow and the real WS2812
 * strip. Every frame it paints, in this order, over the underglow frame:
 *   1. indicator rules (caps/num lock, layers, modifiers, Bluetooth, ...)
 *   2. transient effects (ripple, wave, rain, bloom, ...), added on top
 *
 * The central (left) half decides what is active and tells the peripheral (right)
 * half through the &kfx behavior, so both halves show the same state.
 */

#define DT_DRV_COMPAT keyfx_led_overlay

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/keys.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/position_state_changed.h>

#define CI_IS_CENTRAL (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

#if CI_IS_CENTRAL
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#endif
#endif

#include "keyfx_internal.h"

LOG_MODULE_REGISTER(keyfx, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(keyfx_led_overlay)

/* ------------------------------------------------------------------------- */
/* Vocabulary used by config/keyfx_config.h                               */
/* ------------------------------------------------------------------------- */

enum ci_source {
    CI_END = 0,
    CI_CAPS_ON,      /* Caps Lock is on                                  */
    CI_NUM_OFF,      /* Num Lock is off                                  */
    CI_NUM_ON,       /* Num Lock is on                                   */
    CI_LAYER,        /* layer <arg> is active                            */
    CI_MOD,          /* any modifier in mask <arg> is active (sticky too) */
    CI_CAPS_WORD,    /* caps word is active                              */
    CI_BT_CONNECTED, /* active Bluetooth profile is <arg> and connected  */
    CI_BT_SEARCHING, /* active Bluetooth profile is <arg>, not connected */
    CI_SPLASH_ON,    /* keystroke splash mode is on                      */
};

enum ci_anim { CI_ANIM_SOLID = 0, CI_ANIM_BREATHE, CI_ANIM_BLINK };

#define CI_ALWAYS 0xFF /* "when" value: rule is not tied to a layer */

/* HID modifier masks for CI_MOD */
#define CI_MOD_CTRL 0x11
#define CI_MOD_SHIFT 0x22
#define CI_MOD_ALT 0x44
#define CI_MOD_GUI 0x88
#define CI_MOD_LSHIFT 0x02
#define CI_MOD_RSHIFT 0x20
#define CI_MOD_LCTRL 0x01
#define CI_MOD_RCTRL 0x10
#define CI_MOD_LALT 0x04
#define CI_MOD_RALT 0x40
#define CI_MOD_LGUI 0x08
#define CI_MOD_RGUI 0x80

enum ci_fx_type {
    CI_FX_RIPPLE = 0, /* ring expanding from the origin key over the whole keyboard */
    CI_FX_WAVE,       /* vertical wall sweeping away from the origin key            */
    CI_FX_RAIN,       /* drops falling down a percentage of the LEDs                */
    CI_FX_BLOOM,      /* a percentage of the LEDs swell and fade at random          */
    CI_FX_FLASH,      /* everything flashes and fades                               */
    CI_FX_SPLASH,     /* small quick ripple, meant for keystrokes                   */
};

#define CI_RAINBOW 0xFFFFFFFFu /* effect colour: hue follows position and time */

enum ci_trig_kind { CI_T_END = 0, CI_T_COMBO, CI_T_TAPS, CI_T_PRESS };

/* Effects tied to what ZMK actually runs (see CI_ACTIONS in keyfx_config.h) */
enum ci_act_kind { CI_A_END = 0, CI_A_BEHAVIOR, CI_A_BINDING, CI_A_COMBO };

struct ci_action {
    uint8_t kind;
    uint8_t fx;
    int16_t origin; /* key position the effect starts from, -1 = the pressed key */
    const char *dev;
    uint32_t p1, p2;
};

struct ci_rule {
    uint8_t src;
    uint8_t arg;
    uint8_t anim;
    uint8_t when;
    uint32_t color;
    const uint8_t *pos;
    uint8_t n;
};

struct ci_fx_def {
    uint8_t type;
    uint32_t color;
    uint16_t duration_ms;
    uint16_t arg;
};

struct ci_trigger {
    uint8_t kind;
    uint8_t fx;
    uint8_t count;
    uint16_t window;
    const uint8_t *pos;
    uint8_t n;
};

#define CI_RULE(_src, _arg, _color, _anim, _when, ...)                                             \
    {                                                                                              \
        .src = (_src), .arg = (_arg), .anim = (_anim), .when = (_when), .color = (_color),         \
        .pos = (const uint8_t[]){__VA_ARGS__}, .n = sizeof((const uint8_t[]){__VA_ARGS__}),        \
    },

#define CI_ON_COMBO(_fx, _win, ...)                                                                \
    {                                                                                              \
        .kind = CI_T_COMBO, .fx = FX_##_fx, .window = (_win),                                      \
        .pos = (const uint8_t[]){__VA_ARGS__}, .n = sizeof((const uint8_t[]){__VA_ARGS__}),        \
    },

#define CI_ON_TAPS(_fx, _count, _win, _pos)                                                        \
    {                                                                                              \
        .kind = CI_T_TAPS, .fx = FX_##_fx, .count = (_count), .window = (_win),                    \
        .pos = (const uint8_t[]){_pos}, .n = 1,                                                    \
    },

#define CI_ON_PRESS(_fx, _pos)                                                                     \
    {                                                                                              \
        .kind = CI_T_PRESS, .fx = FX_##_fx, .pos = (const uint8_t[]){_pos}, .n = 1,                \
    },

#define CI_PARAM(_node, _cell)                                                                     \
    COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(_node, bindings, 0, _cell), (0),                            \
                (DT_PHA_BY_IDX(_node, bindings, 0, _cell)))

/* any run of the behavior with this devicetree label (a macro, tap dance, ...) */
#define CI_ON_BEHAVIOR(_fx, _label)                                                                \
    {                                                                                              \
        .kind = CI_A_BEHAVIOR, .fx = FX_##_fx, .origin = -1,                                       \
        .dev = DEVICE_DT_NAME(DT_NODELABEL(_label)),                                               \
    },

/* a run of that behavior with exactly these parameters, e.g. CI_ON_BINDING(X, kp, LC(LA(DEL)), 0) */
#define CI_ON_BINDING(_fx, _label, _p1, _p2)                                                       \
    {                                                                                              \
        .kind = CI_A_BINDING, .fx = FX_##_fx, .origin = -1,                                        \
        .dev = DEVICE_DT_NAME(DT_NODELABEL(_label)), .p1 = (_p1), .p2 = (_p2),                     \
    },

/* a combo declared in the keymap, by its node name in lowercase (LOGOFF -> logoff) */
#define CI_COMBO_NODE(_combo) DT_CHILD(DT_COMPAT_GET_ANY_STATUS_OKAY(zmk_combos), _combo)
#define CI_ON_COMBO_NAME(_fx, _combo)                                                              \
    {                                                                                              \
        .kind = CI_A_COMBO, .fx = FX_##_fx,                                                        \
        .origin = DT_PROP_BY_IDX(CI_COMBO_NODE(_combo), key_positions, 0),                         \
        .dev = DEVICE_DT_NAME(DT_PHANDLE_BY_IDX(CI_COMBO_NODE(_combo), bindings, 0)),              \
        .p1 = CI_PARAM(CI_COMBO_NODE(_combo), param1),                                             \
        .p2 = CI_PARAM(CI_COMBO_NODE(_combo), param2),                                             \
    },

#define CI_EFFECT(_name, _type, _color, _ms, _arg) {_type, _color, _ms, _arg},

#include "keyfx_config.h"

#ifndef CI_ACTIONS
#define CI_ACTIONS
#endif

enum {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) FX_##_name,
    CI_EFFECTS
        FX_COUNT
};

static const struct ci_fx_def fx_defs[] = {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) {_type, _color, _ms, _arg},
    CI_EFFECTS};

static const char *const fx_names[] = {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) #_name,
    CI_EFFECTS};

static const struct ci_rule rules[] = {CI_RULES{.src = CI_END}};

static const struct ci_trigger triggers[] = {CI_TRIGGERS{.kind = CI_T_END}};
#define CI_TRIGGER_COUNT (ARRAY_SIZE(triggers) - 1)

static const uint8_t splash_styles[] = {CI_SPLASH_STYLES};
#define CI_SPLASH_STYLE_COUNT ARRAY_SIZE(splash_styles)

/* ------------------------------------------------------------------------- */
/* Tunables with defaults                                                    */
/* ------------------------------------------------------------------------- */

#ifndef CI_BREATHE_MS
#define CI_BREATHE_MS 2400
#endif
#ifndef CI_BLINK_MS
#define CI_BLINK_MS 700
#endif
#ifndef CI_FRAME_MS
#define CI_FRAME_MS 25
#endif
#ifndef CI_FX_BRIGHTNESS_PERCENT
#define CI_FX_BRIGHTNESS_PERCENT 60
#endif
#ifndef CI_SPLASH_DEFAULT_ON
#define CI_SPLASH_DEFAULT_ON 0
#endif
#define CI_LEFT 0
#define CI_RIGHT 1
#define CI_KEY(_half, _led, _x, _y) {(_half), (_led), (_x), (_y)},
#define CI_EXTRA(_half, _led, _x, _y) {(_half), (_led), (_x), (_y)},

struct ci_geo {
    uint8_t half;
    uint8_t led;
    int16_t x, y;
};

static const struct ci_geo geo_keys[] = {CI_KEYS};
static const struct ci_geo geo_extra[] = {CI_EXTRA_LEDS};

#define CI_POSITIONS ARRAY_SIZE(geo_keys)
#define CI_LOCAL_HALF (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) ? CI_LEFT : CI_RIGHT)
#define CI_N DT_INST_PROP(0, chain_length)
#define CI_MAX_FX 10

/* USB HID LED report bits */
#define CI_NUMLOCK_BIT BIT(0)
#define CI_CAPSLOCK_BIT BIT(1)

/* st_flags layout (also what the central sends to the peripheral) */
#define ST_CAPS_WORD BIT(0)
#define ST_BT_CONNECTED BIT(1)
#define ST_SPLASH_ON BIT(2)
#define ST_IND_ON BIT(3)
#define ST_PROFILE_SHIFT 4 /* 3 bits: active Bluetooth profile */

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

struct fx_inst {
    int8_t id; /* -1 = free */
    int64_t start;
    int16_t ox, oy;
    uint32_t seed;
};

struct ov_config {
    const struct device *strip;
};

static const struct ov_config ov_cfg = {
    .strip = DEVICE_DT_GET(DT_INST_PROP(0, strip)),
};

static K_MUTEX_DEFINE(ov_lock);
static struct led_rgb last_frame[CI_N];
static struct led_rgb out_frame[CI_N];
static int16_t lx[CI_N], ly[CI_N];
static struct fx_inst fx_slots[CI_MAX_FX];
static bool have_frame;
static bool ov_ready;

static uint8_t hid_ind;
static uint8_t st_layers;
static uint8_t st_mods;
static uint8_t st_flags = (CI_SPLASH_DEFAULT_ON ? ST_SPLASH_ON : 0) | ST_IND_ON;
static uint8_t splash_idx;

static void kick(void);

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static int32_t isqrt32(uint32_t v) {
    uint32_t r = 0, b = 1u << 30;

    while (b > v) {
        b >>= 2;
    }
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return (int32_t)r;
}

/* Key position -> grid coordinates in tenths of a key pitch. */
static void pos_xy(uint8_t pos, int16_t *x, int16_t *y) {
    if (pos < CI_POSITIONS) {
        *x = geo_keys[pos].x;
        *y = geo_keys[pos].y;
    } else {
        *x = *y = 0;
    }
}

/* Key position -> LED index on this half's strip, or -1 if the key is on the other half. */
static int pos_led(uint8_t pos) {
    if (pos >= CI_POSITIONS || geo_keys[pos].half != CI_LOCAL_HALF || geo_keys[pos].led >= CI_N) {
        return -1;
    }
    return geo_keys[pos].led;
}

static void build_geometry(void) {
    for (size_t i = 0; i < CI_POSITIONS; i++) {
        if (pos_led(i) >= 0) {
            lx[geo_keys[i].led] = geo_keys[i].x;
            ly[geo_keys[i].led] = geo_keys[i].y;
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(geo_extra); i++) {
        if (geo_extra[i].half == CI_LOCAL_HALF && geo_extra[i].led < CI_N) {
            lx[geo_extra[i].led] = geo_extra[i].x;
            ly[geo_extra[i].led] = geo_extra[i].y;
        }
    }
}

static struct led_rgb hue_rgb(uint16_t h) {
    struct led_rgb px = {0};
    uint8_t region = (h % 360) / 60;
    uint8_t rem = ((h % 60) * 255) / 60;

    switch (region) {
    case 0: px.r = 255; px.g = rem; break;
    case 1: px.r = 255 - rem; px.g = 255; break;
    case 2: px.g = 255; px.b = rem; break;
    case 3: px.g = 255 - rem; px.b = 255; break;
    case 4: px.r = rem; px.b = 255; break;
    default: px.r = 255; px.b = 255 - rem; break;
    }
    return px;
}

static struct led_rgb rgb_from(uint32_t c, uint32_t level255, uint32_t pct) {
    struct led_rgb px = {0};
    uint32_t k = level255 * pct; /* 0..25500 */

    px.r = (((c >> 16) & 0xFF) * k) / 25500;
    px.g = (((c >> 8) & 0xFF) * k) / 25500;
    px.b = ((c & 0xFF) * k) / 25500;
    return px;
}

static uint8_t anim_factor(uint8_t anim, int64_t now) {
    switch (anim) {
    case CI_ANIM_BREATHE: {
        uint32_t p = now % CI_BREATHE_MS;
        uint32_t half = CI_BREATHE_MS / 2;
        uint32_t v = p < half ? (p * 255) / half : ((CI_BREATHE_MS - p) * 255) / half;
        v = (v * v) / 255; /* ease */
        return 15 + (v * 240) / 255;
    }
    case CI_ANIM_BLINK:
        return (now % CI_BLINK_MS) < (CI_BLINK_MS / 2) ? 255 : 0;
    default:
        return 255;
    }
}

/* ------------------------------------------------------------------------- */
/* Effects                                                                   */
/* ------------------------------------------------------------------------- */

static void fx_start(uint8_t id, uint8_t origin_pos, uint32_t seed) {
    if (id >= FX_COUNT) {
        return;
    }

    int slot = -1;
    int oldest = 0;
    int64_t now = k_uptime_get();

    for (int i = 0; i < CI_MAX_FX; i++) {
        if (fx_slots[i].id < 0) {
            slot = i;
            break;
        }
        if (fx_slots[i].start < fx_slots[oldest].start) {
            oldest = i;
        }
    }
    if (slot < 0) {
        slot = oldest;
    }

    fx_slots[slot].id = id;
    fx_slots[slot].start = now;
    fx_slots[slot].seed = seed;
    pos_xy(origin_pos, &fx_slots[slot].ox, &fx_slots[slot].oy);
}

/* Returns the brightness (0-255) of effect `f` at LED `led`, `t` ms after it started. */
static uint32_t fx_level(const struct ci_fx_def *d, const struct fx_inst *f, int led, int32_t t) {
    int32_t dur = d->duration_ms;
    int32_t dx = lx[led] - f->ox;
    int32_t dy = ly[led] - f->oy;

    if (dur <= 0 || t < 0 || t > dur) {
        return 0;
    }

    switch (d->type) {
    case CI_FX_RIPPLE:
    case CI_FX_SPLASH: {
        bool small = d->type == CI_FX_SPLASH;
        int32_t maxr = d->arg ? (int32_t)d->arg : (small ? 30 : 135);
        int32_t w = small ? 14 : 20;
        int32_t dist = isqrt32((uint32_t)(dx * dx + dy * dy));
        int32_t r = ((maxr + w) * t) / dur;
        int32_t diff = abs(dist - r);

        if (diff >= w) {
            return 0;
        }
        uint32_t lvl = (255 * (w - diff)) / w;
        uint32_t fade = 255 - (255 * t) / dur;
        return (lvl * fade) / 255;
    }
    case CI_FX_WAVE: {
        const int32_t tail = 45;
        int32_t front = ((120 + tail) * t) / dur;
        int32_t behind = front - abs(dx);

        if (behind < 0 || behind > tail) {
            return 0;
        }
        uint32_t lvl = (255 * (tail - behind)) / tail;
        uint32_t fade = 255 - (255 * t) / (2 * dur);
        return (lvl * fade) / 255;
    }
    case CI_FX_RAIN: {
        uint32_t h = hash32((uint32_t)led * 2654435761u ^ f->seed);
        uint32_t pct = d->arg ? d->arg : 30;

        if ((h % 100) >= pct) {
            return 0;
        }
        int32_t life = (dur * 35) / 100;
        int32_t base = ((ly[led] + 10) * ((dur * 55) / 100)) / 50;
        int32_t jitter = (h >> 8) % ((dur * 10) / 100 + 1);
        int32_t u = t - base - jitter;

        if (u < 0 || u > life || life <= 0) {
            return 0;
        }
        return 255 - (255 * u) / life;
    }
    case CI_FX_BLOOM: {
        uint32_t h = hash32((uint32_t)led * 2246822519u ^ f->seed);
        uint32_t pct = d->arg ? d->arg : 30;

        if ((h % 100) >= pct) {
            return 0;
        }
        int32_t life = dur / 2;
        int32_t start = (h >> 8) % (dur / 2 + 1);
        int32_t u = t - start;
        int32_t half = life / 2;

        if (u < 0 || u > life || half <= 0) {
            return 0;
        }
        return u < half ? (255 * u) / half : (255 * (life - u)) / half;
    }
    case CI_FX_FLASH:
        return 255 - (255 * t) / dur;
    default:
        return 0;
    }
}

/* Paints all running effects onto out_frame; returns true while any is running. */
static bool paint_effects(int64_t now) {
    bool running = false;

    for (int i = 0; i < CI_MAX_FX; i++) {
        struct fx_inst *f = &fx_slots[i];

        if (f->id < 0) {
            continue;
        }
        const struct ci_fx_def *d = &fx_defs[f->id];
        int32_t t = (int32_t)(now - f->start);

        if (t > d->duration_ms) {
            f->id = -1;
            continue;
        }
        running = true;

        for (int led = 0; led < CI_N; led++) {
            uint32_t lvl = fx_level(d, f, led, t);
            if (!lvl) {
                continue;
            }
            struct led_rgb add;
            if (d->color == CI_RAINBOW) {
                struct led_rgb base = hue_rgb((lx[led] * 2 + t / 5) % 360);
                add = rgb_from(((uint32_t)base.r << 16) | ((uint32_t)base.g << 8) | base.b, lvl,
                               CI_FX_BRIGHTNESS_PERCENT);
            } else {
                add = rgb_from(d->color, lvl, CI_FX_BRIGHTNESS_PERCENT);
            }
            out_frame[led].r = MIN(255, out_frame[led].r + add.r);
            out_frame[led].g = MIN(255, out_frame[led].g + add.g);
            out_frame[led].b = MIN(255, out_frame[led].b + add.b);
        }
    }
    return running;
}

/* ------------------------------------------------------------------------- */
/* Indicator rules                                                           */
/* ------------------------------------------------------------------------- */

static bool rule_active(const struct ci_rule *r) {
    uint8_t profile = (st_flags >> ST_PROFILE_SHIFT) & 0x7;

    if (r->when != CI_ALWAYS && !(st_layers & BIT(r->when & 7))) {
        return false;
    }

    switch (r->src) {
    case CI_CAPS_ON:
        return hid_ind & CI_CAPSLOCK_BIT;
    case CI_NUM_OFF:
        return !(hid_ind & CI_NUMLOCK_BIT);
    case CI_NUM_ON:
        return hid_ind & CI_NUMLOCK_BIT;
    case CI_LAYER:
        return r->arg < 8 && (st_layers & BIT(r->arg));
    case CI_MOD:
        return st_mods & r->arg;
    case CI_CAPS_WORD:
        return st_flags & ST_CAPS_WORD;
    case CI_BT_CONNECTED:
        return profile == r->arg && (st_flags & ST_BT_CONNECTED);
    case CI_BT_SEARCHING:
        return profile == r->arg && !(st_flags & ST_BT_CONNECTED);
    case CI_SPLASH_ON:
        return st_flags & ST_SPLASH_ON;
    default:
        return false;
    }
}

/* Paints indicator rules onto out_frame; returns true if any lit rule is animated. */
static bool paint_rules(int64_t now, bool *any_lit) {
    bool animated = false;

    *any_lit = false;
    if (!(st_flags & ST_IND_ON)) {
        return false;
    }

    for (const struct ci_rule *r = rules; r->src != CI_END; r++) {
        if (!rule_active(r)) {
            continue;
        }
        uint8_t factor = anim_factor(r->anim, now);

        if (r->anim != CI_ANIM_SOLID) {
            animated = true;
        }
        for (uint8_t i = 0; i < r->n; i++) {
            int led = pos_led(r->pos[i]);

            if (led >= 0) {
                out_frame[led] = rgb_from(r->color, factor, CI_BRIGHTNESS_PERCENT);
                *any_lit = true;
            }
        }
    }
    return animated;
}

/* ------------------------------------------------------------------------- */
/* Frame output                                                              */
/* ------------------------------------------------------------------------- */

/* Caller holds ov_lock. Returns true while animation needs more frames. */
static bool push_frame(int *rc_out) {
    int64_t now = k_uptime_get();
    bool any_lit;

    memcpy(out_frame, last_frame, sizeof(out_frame));
    bool animated = paint_rules(now, &any_lit);
    bool fx_running = paint_effects(now);

    *rc_out = led_strip_update_rgb(ov_cfg.strip, out_frame, CI_N);
    return animated || fx_running;
}

static void tick_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tick_work, tick_fn);

static void tick_fn(struct k_work *work) {
    int rc = 0;

    if (!ov_ready) {
        return;
    }
    k_mutex_lock(&ov_lock, K_FOREVER);
    if (!have_frame) {
        memset(last_frame, 0, sizeof(last_frame));
        have_frame = true;
    }
    bool again = push_frame(&rc);
    k_mutex_unlock(&ov_lock);

    if (rc) {
        LOG_WRN("strip update failed (%d)", rc);
    }
    if (again) {
        k_work_reschedule(&tick_work, K_MSEC(CI_FRAME_MS));
    }
}

static void kick(void) {
    k_work_reschedule(&tick_work, K_NO_WAIT);
}

/* ---- led_strip driver API (what ZMK underglow talks to) ---- */

static int ov_update_rgb(const struct device *dev, struct led_rgb *pixels, size_t num_pixels) {
    ARG_UNUSED(dev);
    int rc = 0;

    k_mutex_lock(&ov_lock, K_FOREVER);
    size_t n = MIN(num_pixels, (size_t)CI_N);
    memcpy(last_frame, pixels, n * sizeof(struct led_rgb));
    have_frame = true;
    bool again = push_frame(&rc);
    k_mutex_unlock(&ov_lock);

    if (again) {
        k_work_reschedule(&tick_work, K_MSEC(CI_FRAME_MS));
    }
    return rc;
}

static size_t ov_length(const struct device *dev) {
    ARG_UNUSED(dev);
    return CI_N;
}

static const struct led_strip_driver_api ov_api = {
    .update_rgb = ov_update_rgb,
    .length = ov_length,
};

/* ------------------------------------------------------------------------- */
/* Commands (from keys, or from the central)                                 */
/* ------------------------------------------------------------------------- */

void keyfx_engine_command(uint32_t p1, uint32_t p2) {
    uint8_t op = p1 >> 24;

    if (op == KEYFX_OP_CMD) {
        switch (p1 & 0xFFFFFF) {
        case 1: /* KFX_SPLASH_TOG */
            st_flags ^= ST_SPLASH_ON;
            break;
        case 2: /* KFX_SPLASH_ON */
            st_flags |= ST_SPLASH_ON;
            break;
        case 3: /* KFX_SPLASH_OFF */
            st_flags &= ~ST_SPLASH_ON;
            break;
        case 4: /* KFX_SPLASH_NEXT */
            if (CI_SPLASH_STYLE_COUNT) {
                splash_idx = (splash_idx + 1) % CI_SPLASH_STYLE_COUNT;
            }
            break;
        case 5: /* KFX_IND_TOG */
            st_flags ^= ST_IND_ON;
            break;
        case 6: /* KFX_PLAY */
            k_mutex_lock(&ov_lock, K_FOREVER);
            fx_start(p2 & 0xFF, (p2 >> 8) & 0xFF, hash32((uint32_t)k_uptime_get()));
            k_mutex_unlock(&ov_lock);
            break;
        default:
            break;
        }
        kick();
        return;
    }

#if !CI_IS_CENTRAL
    if (op == KEYFX_OP_STATE) {
        st_layers = p1 & 0xFF;
        st_mods = (p1 >> 8) & 0xFF;
        st_flags = (p1 >> 16) & 0xFF;
        hid_ind = p2 & 0xFF;
        kick();
    } else if (op == KEYFX_OP_FX) {
        k_mutex_lock(&ov_lock, K_FOREVER);
        fx_start((p1 >> 16) & 0xFF, p1 & 0xFF, p2);
        k_mutex_unlock(&ov_lock);
        kick();
    }
#endif
}

static int ov_init(const struct device *dev) {
    ARG_UNUSED(dev);

    if (!device_is_ready(ov_cfg.strip)) {
        LOG_ERR("LED strip not ready");
        return -ENODEV;
    }
    for (int i = 0; i < CI_MAX_FX; i++) {
        fx_slots[i].id = -1;
    }
    build_geometry();
    ov_ready = true;
    return 0;
}

DEVICE_DT_INST_DEFINE(0, ov_init, NULL, NULL, NULL, POST_KERNEL, CONFIG_LED_STRIP_INIT_PRIORITY,
                      &ov_api);

/* ------------------------------------------------------------------------- */
/* Events                                                                    */
/* ------------------------------------------------------------------------- */

static int on_hid_indicators(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);

    if (ev) {
        hid_ind = ev->indicators;
        kick();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(aaa_keyfx_hid, on_hid_indicators);
ZMK_SUBSCRIPTION(aaa_keyfx_hid, zmk_hid_indicators_changed);

#if CI_IS_CENTRAL

/* ---- central: gather state and tell the peripheral ---- */

static bool caps_word_active(void) {
#if IS_ENABLED(CONFIG_KEYFX_CAPS_WORD_PEEK) && DT_NODE_EXISTS(DT_NODELABEL(caps_word))
    const struct device *cw = DEVICE_DT_GET(DT_NODELABEL(caps_word));
    /* the behavior keeps its state as the first member of its data: bool active */
    return cw && cw->data && *(const bool *)cw->data;
#else
    return false;
#endif
}

static void send_state(void) {
    keyfx_sync_send(((uint32_t)KEYFX_OP_STATE << 24) | ((uint32_t)st_flags << 16) |
                       ((uint32_t)st_mods << 8) | st_layers,
                   hid_ind);
}

static void check_state(bool force) {
    uint8_t layers = 0;

    for (int i = 0; i < 8; i++) {
        if (zmk_keymap_layer_active(i)) {
            layers |= BIT(i);
        }
    }
    uint8_t mods = zmk_hid_get_explicit_mods();
    uint8_t flags = st_flags & (ST_SPLASH_ON | ST_IND_ON);

    if (caps_word_active()) {
        flags |= ST_CAPS_WORD;
    }
#if IS_ENABLED(CONFIG_ZMK_BLE)
    flags |= (zmk_ble_active_profile_index() & 0x7) << ST_PROFILE_SHIFT;
    if (zmk_ble_active_profile_is_connected()) {
        flags |= ST_BT_CONNECTED;
    }
#endif

    bool changed = layers != st_layers || mods != st_mods || flags != st_flags;

    st_layers = layers;
    st_mods = mods;
    st_flags = flags;

    if (changed || force) {
        send_state();
    }
    if (changed) {
        kick();
    }
}

static void state_fn(struct k_work *work) {
    check_state(false);
}
static K_WORK_DELAYABLE_DEFINE(state_work, state_fn);

static void beat_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(beat_work, beat_fn);

static void beat_fn(struct k_work *work) {
    /* periodic resend heals a peripheral that reconnected or rebooted */
    check_state(true);
    k_work_reschedule(&beat_work, K_SECONDS(2));
}

static void schedule_state_check(void) {
    k_work_schedule(&state_work, K_MSEC(10));
}

static int on_state_event(const zmk_event_t *eh) {
    schedule_state_check();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(aaa_keyfx_state, on_state_event);
ZMK_SUBSCRIPTION(aaa_keyfx_state, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(aaa_keyfx_state, zmk_keycode_state_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(aaa_keyfx_state, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(aaa_keyfx_state, zmk_endpoint_changed);
#endif

/* ---- central: key presses -> triggers and splash ---- */

static bool key_down[CI_POSITIONS];
static int64_t last_press[CI_POSITIONS];
static int64_t trig_last[CI_TRIGGER_COUNT + 1];
static uint8_t trig_count[CI_TRIGGER_COUNT + 1];

static void trigger_fx(uint8_t id, uint8_t origin_pos) {
    uint32_t seed = hash32((uint32_t)k_uptime_get());

    k_mutex_lock(&ov_lock, K_FOREVER);
    fx_start(id, origin_pos, seed);
    k_mutex_unlock(&ov_lock);
    keyfx_sync_send(((uint32_t)KEYFX_OP_FX << 24) | ((uint32_t)id << 16) | origin_pos, seed);
    kick();
}

/* ---- effects tied to ZMK actions ---- */

static const struct ci_action actions[] = {CI_ACTIONS{.kind = CI_A_END}};

struct pending_fx {
    uint8_t fx;
    uint8_t origin;
};
K_MSGQ_DEFINE(act_q, sizeof(struct pending_fx), 8, 1);

static void act_fn(struct k_work *work) {
    struct pending_fx p;

    while (k_msgq_get(&act_q, &p, K_NO_WAIT) == 0) {
        trigger_fx(p.fx, p.origin);
    }
}
static K_WORK_DEFINE(act_work, act_fn);

void keyfx_on_binding(const struct zmk_behavior_binding *b,
                     const struct zmk_behavior_binding_event *ev) {
    static const char kfx_name[] = DEVICE_DT_NAME(DT_NODELABEL(kfx));

    if (!b->behavior_dev || strcmp(b->behavior_dev, kfx_name) == 0) {
        return; /* our own sync messages */
    }
    bool virtual_pos = ev->position >= CI_POSITIONS;

    for (const struct ci_action *a = actions; a->kind != CI_A_END; a++) {
        if (strcmp(a->dev, b->behavior_dev) != 0) {
            continue;
        }
        bool same_params = a->p1 == b->param1 && a->p2 == b->param2;

        if ((a->kind == CI_A_BINDING && !same_params) ||
            (a->kind == CI_A_COMBO && !(virtual_pos && same_params))) {
            continue;
        }
        struct pending_fx p = {
            .fx = a->fx,
            .origin = a->origin >= 0 ? (uint32_t)a->origin : (virtual_pos ? 0u : ev->position),
        };
        if (k_msgq_put(&act_q, &p, K_NO_WAIT) == 0) {
            k_work_submit(&act_work);
        }
    }
}

void keyfx_play_by_name(const char *name, uint8_t origin_pos) {
    for (int i = 0; i < FX_COUNT; i++) {
        if (strcmp(fx_names[i], name) == 0) {
            trigger_fx(i, origin_pos);
            return;
        }
    }
    LOG_WRN("unknown light effect '%s'", name);
}

static bool pos_in(const struct ci_trigger *t, uint8_t p) {
    for (uint8_t i = 0; i < t->n; i++) {
        if (t->pos[i] == p) {
            return true;
        }
    }
    return false;
}

static void handle_press(uint8_t p, int64_t now) {
    bool fired = false;

    last_press[p] = now;
    key_down[p] = true;

    for (size_t i = 0; i < CI_TRIGGER_COUNT; i++) {
        const struct ci_trigger *t = &triggers[i];

        if (t->kind == CI_T_COMBO && pos_in(t, p)) {
            bool all = true;

            for (uint8_t k = 0; k < t->n && all; k++) {
                uint8_t q = t->pos[k];
                if (q != p && !(key_down[q] || (now - last_press[q]) <= t->window)) {
                    all = false;
                }
            }
            if (all && (now - trig_last[i]) > (int64_t)t->window + 200) {
                trig_last[i] = now;
                trigger_fx(t->fx, t->pos[0]);
                fired = true;
            }
        } else if (t->kind == CI_T_PRESS && t->pos[0] == p) {
            trigger_fx(t->fx, p);
            fired = true;
        }
    }

    if (fired) {
        memset(trig_count, 0, sizeof(trig_count));
    } else {
        for (size_t i = 0; i < CI_TRIGGER_COUNT; i++) {
            const struct ci_trigger *t = &triggers[i];

            if (t->kind != CI_T_TAPS || t->pos[0] != p) {
                continue;
            }
            if ((now - trig_last[i]) <= t->window) {
                trig_count[i]++;
            } else {
                trig_count[i] = 1;
            }
            trig_last[i] = now;
            if (trig_count[i] >= t->count) {
                trig_count[i] = 0;
                trigger_fx(t->fx, p);
                fired = true;
            }
        }
    }

    if (!fired && (st_flags & ST_SPLASH_ON) && CI_SPLASH_STYLE_COUNT) {
        trigger_fx(splash_styles[splash_idx % CI_SPLASH_STYLE_COUNT], p);
    }
}

static int on_position(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev && ev->position < CI_POSITIONS) {
        if (ev->state) {
            handle_press((uint8_t)ev->position, k_uptime_get());
        } else {
            key_down[ev->position] = false;
        }
        schedule_state_check();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

/* The name sorts before ZMK's own listeners so presses are seen before combos or
 * hold-taps can capture them. */
ZMK_LISTENER(aaa_keyfx_pos, on_position);
ZMK_SUBSCRIPTION(aaa_keyfx_pos, zmk_position_state_changed);

static int keyfx_central_init(void) {
    k_work_schedule(&beat_work, K_SECONDS(3));
    return 0;
}
SYS_INIT(keyfx_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#else /* peripheral half: actions run on the central, which sends the effect over */

void keyfx_play_by_name(const char *name, uint8_t origin_pos) {
    ARG_UNUSED(name);
    ARG_UNUSED(origin_pos);
}

#endif /* CI_IS_CENTRAL */

#endif /* DT_HAS_COMPAT_STATUS_OKAY(keyfx_led_overlay) */
