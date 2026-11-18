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
    CI_HELD,         /* key <hold_pos> has been held for <hold_ms>       */
};

enum ci_anim { CI_ANIM_SOLID = 0, CI_ANIM_BREATHE, CI_ANIM_BLINK };

#define CI_ALWAYS 0xFF /* "when" value: rule is not tied to a layer */
#define CI_ONLY(_n) (0x40 | (_n)) /* "when" value: only while layer _n is the highest active one */

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
    CI_FX_BEAM,       /* laser: one or more shots from a key, at an angle or to a key */
    CI_FX_SNAKE,      /* a wiggling trail travelling from a start key to an end key   */
    CI_FX_SPIRAL,     /* a trail spiralling out from (or, inverted, into) a key       */
    CI_FX_BALL,       /* a bouncing ball with a trail                                 */
    CI_FX_LIGHTNING,  /* a jagged bolt with random brightness and flicker             */
    CI_FX_SWIPE,      /* a wall of light sweeping in any direction                    */
    CI_FX_MATRIX,     /* columns of falling light with a shimmering shine             */
    CI_FX_MIX,        /* a mix of other effects (see CI_MIXES)                      */
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
    uint8_t hold_pos; /* CI_HELD: the key that has to be held       */
    uint16_t hold_ms; /* CI_HELD: for how long before the lights come on */
    /* optional per-rule settings (0 = use the general setting); see CI_RULEX */
    uint8_t brightness;  /* percent                                            */
    uint16_t period_ms;  /* breathe / blink period                             */
    uint16_t fade_in_ms; /* time to come up to full brightness                 */
    uint16_t fade_out_ms; /* time to fade away when the condition ends         */
    uint32_t color2, color3; /* more colours: a gradient across the listed keys */
};

struct ci_fx_def {
    uint8_t type;
    uint32_t color;
    uint16_t duration_ms;
    uint16_t arg;
    /* motion effects (see CI_EFFECTX in keyfx_config.h); 0 = default / not used */
    uint8_t from;   /* start key, CI_AT(pos); 0 = the key that triggered the effect */
    uint8_t to;     /* end key, CI_AT(pos); sets direction and distance             */
    int16_t angle;  /* direction in degrees: 0 right, 90 down, 180 left, 270 up     */
    uint16_t dist;  /* distance / radius in tenths of a key (0 = automatic)         */
    uint8_t count;  /* shots (beam), bolts (lightning), % of columns (matrix)       */
    uint16_t spread; /* degrees covered by the shots of a fan                       */
    uint8_t size;   /* trail length, percent of the path                            */
    uint8_t width;  /* thickness, tenths of a key                                   */
    uint8_t wiggle; /* sideways amount (snake, lightning)                           */
    uint8_t turns;  /* spiral turns                                                 */
    uint16_t height; /* ball drop height, tenths of a key                           */
    uint8_t bounce; /* ball bounciness, percent of height kept at each bounce       */
    int8_t accel;   /* -100..100: + starts slow and ends fast, - the opposite       */
    bool invert;    /* run backwards (converge instead of spread)                   */
    bool random;    /* random direction every time                                  */
    bool bidir;     /* swipe: spread both ways from the start line                  */
    bool fill;      /* swipe: leave what the front passed lit until the end         */
    /* matrix only */
    uint32_t head;  /* colour of the leading light of every column (0 = none)       */
    uint8_t count_max;  /* up to this % of columns (.count = the least)             */
    uint8_t speed_min, speed_max; /* fall speed per column, percent (70..130)       */
    uint8_t flicker; /* flicker strength percent (default 40)                       */
};

/* one part of a mix: an existing effect with its own timing and look (see CI_MIXES) */
struct ci_part {
    uint8_t fx;           /* the effect to play (FX_ id, set by CI_PART)           */
    uint16_t start_ms;    /* when it starts, from the start of the mix (a pause)   */
    uint16_t duration_ms; /* play the effect over this long instead (0 = normal)   */
    uint16_t speed;       /* percent, 100 = normal                                 */
    int16_t accel;        /* change of speed in percent per second (+ faster, - slower) */
    uint8_t brightness;   /* percent of the effect brightness (0 = 100)            */
    uint16_t fade_in_ms, fade_out_ms;
    uint16_t pulse_ms;    /* flash on and off with this period                     */
    uint16_t breathe_ms;  /* swell and dim with this period                        */
    uint8_t repeat;       /* number of times (0 = once)                            */
    uint16_t pause_ms;    /* pause between repeats                                 */
    uint8_t origin;       /* CI_AT(position): start from this key (0 = the trigger's) */
    uint32_t color, color2; /* colour override; with color2 a left-to-right gradient */
    uint16_t len;         /* worked out at start-up                                */
};

struct ci_mix {
    struct ci_part *parts;
    uint8_t n;
};

#define CI_AT(_pos) ((_pos) + 1)
#define CI_PART(_fx, ...) {.fx = FX_##_fx, __VA_ARGS__},

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

/* Lights up <keys...> while position <hold_pos> is held down for at least <delay_ms>.
 * The held key itself only lights if it is also in the list. */
#define CI_HELD_RULE(_hold_pos, _delay_ms, _color, _anim, _when, ...)                              \
    {                                                                                              \
        .src = CI_HELD, .hold_pos = (_hold_pos), .hold_ms = (_delay_ms), .anim = (_anim),          \
        .when = (_when), .color = (_color), .pos = (const uint8_t[]){__VA_ARGS__},                 \
        .n = sizeof((const uint8_t[]){__VA_ARGS__}),                                               \
    },

/* CI_RULE with extra settings:
 *   CI_RULEX(src, arg, colour, anim, layer, CI_OPTS(.brightness = 40, .fade_in_ms = 300), keys...)
 * options: .brightness (%), .period_ms, .fade_in_ms, .fade_out_ms, .color2, .color3 */
#define CI_OPTS(...) __VA_ARGS__,
#define CI_RULEX(_src, _arg, _color, _anim, _when, _opts, ...)                                     \
    {                                                                                              \
        .src = (_src), .arg = (_arg), .anim = (_anim), .when = (_when), .color = (_color), _opts   \
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
#define CI_EFFECTX(_name, _type, _color, _ms, ...) {.type = _type, .color = _color, .duration_ms = _ms, __VA_ARGS__},

#include "keyfx_config.h"

#define FX_NONE 0xFF /* "no effect" in the leader settings */

#ifndef CI_LEADERS
#define CI_LEADERS
#endif
#ifndef CI_LEADER_SEQS
#define CI_LEADER_SEQS
#endif
#ifndef CI_LEADER_KEYS
#define CI_LEADER_KEYS
#endif
#ifndef CI_LEADER_IGNORE_POS
#define CI_LEADER_IGNORE_POS /* key positions that never abort a sequence (layer keys) */
#endif
#ifndef CI_LEADER_DIM_OTHERS
#define CI_LEADER_DIM_OTHERS 1 /* 1 = turn every other light off while a sequence runs */
#endif
#ifndef CI_LEADER_ANIM
#define CI_LEADER_ANIM CI_ANIM_SOLID
#endif
#ifndef CI_LEADER_TIMEOUT_MS
/* Lights go out after this long without a listed key, unless the leader behavior has its own
 * timeout-ms. 0 = never. */
#define CI_LEADER_TIMEOUT_MS 15000
#endif
#ifndef CI_LEADER_STRICT
/* 0 = keys that are not in CI_LEADER_KEYS are ignored; 1 = they abort the sequence */
#define CI_LEADER_STRICT 0
#endif

#ifndef CI_ACTIONS
#define CI_ACTIONS
#endif

#ifndef CI_MIXES
#define CI_MIXES
#endif

enum {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) FX_##_name,
#undef CI_EFFECTX
#define CI_EFFECTX(_name, _type, _color, _ms, ...) FX_##_name,
    CI_EFFECTS
        FX_BASE_COUNT
};

enum {
    FX_MIXES_START_ = FX_BASE_COUNT - 1, /* the first mix is numbered FX_BASE_COUNT */
#undef CI_MIX
#define CI_MIX(_name, ...) FX_##_name,
    CI_MIXES
        FX_COUNT
};

static struct ci_fx_def fx_defs[] = {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) {_type, _color, _ms, _arg},
#undef CI_EFFECTX
#define CI_EFFECTX(_name, _type, _color, _ms, ...) {.type = _type, .color = _color, .duration_ms = _ms, __VA_ARGS__},
    CI_EFFECTS
#undef CI_MIX
#define CI_MIX(_name, ...) [FX_##_name] = {CI_FX_MIX, 0, 0, FX_##_name - FX_BASE_COUNT},
        CI_MIXES};

static const char *const fx_names[] = {
#undef CI_EFFECT
#define CI_EFFECT(_name, _type, _color, _ms, _arg) #_name,
#undef CI_EFFECTX
#define CI_EFFECTX(_name, _type, _color, _ms, ...) #_name,
    CI_EFFECTS
#undef CI_MIX
#define CI_MIX(_name, ...) [FX_##_name] = #_name,
        CI_MIXES};

/* the parts of every mix, then the table of mixes */
#undef CI_MIX
#define CI_MIX(_name, ...) static struct ci_part mixparts_##_name[] = {__VA_ARGS__};
CI_MIXES
#undef CI_MIX
#define CI_MIX(_name, ...) {mixparts_##_name, ARRAY_SIZE(mixparts_##_name)},
static struct ci_mix mixes[] = {CI_MIXES{NULL, 0}};

static const struct ci_rule rules[] = {CI_RULES{.src = CI_END}};

static const struct ci_trigger triggers[] = {CI_TRIGGERS{.kind = CI_T_END}};
#define CI_TRIGGER_COUNT (ARRAY_SIZE(triggers) - 1)

static const uint8_t splash_styles[] = {CI_SPLASH_STYLES};
#define CI_SPLASH_STYLE_COUNT ARRAY_SIZE(splash_styles)

/* ---- leader key lighting: sequences are read from the keymap at build time ---- */

#define CI_LD_MAX_SEQ 32
#define CI_LD_MAX_PAL 15

struct ci_seq {
    const char *name;
    uint8_t len;
    const uint32_t *codes;
    uint8_t pal; /* palette entry, filled in at start-up */
    uint8_t fx;  /* effect when this sequence completes */
};

struct ci_leader {
    const char *dev;
    struct ci_seq *seqs;
    uint8_t nseq;
    uint8_t done_fx, abort_fx;
    const uint32_t *ignore;
    uint8_t n_ignore;
    uint16_t timeout_ms;
};

struct ci_seqcfg {
    const char *name;
    uint32_t color;
    uint8_t fx;
};

/* where each leader-sequence key sits, per layer (CI_LEADER_KEYS in the config) */
struct ci_lk {
    uint32_t code;
    uint8_t pos;
};
struct ci_lkl {
    uint8_t layer;
    const struct ci_lk *keys;
    uint8_t n;
};
#define CI_LKEY(_code, _pos) {(_code), (_pos)}
#define CI_LKEYS(_layer, ...)                                                                      \
    {                                                                                              \
        .layer = (_layer), .keys = (const struct ci_lk[]){__VA_ARGS__},                            \
        .n = sizeof((const struct ci_lk[]){__VA_ARGS__}) / sizeof(struct ci_lk),                   \
    },
static const struct ci_lkl lkeys[] = {CI_LEADER_KEYS{.keys = NULL}};
static const uint8_t ld_ignore_pos[] = {CI_LEADER_IGNORE_POS 0xFF};

#define CI_LD_CODE(_n, _prop, _i) DT_PROP_BY_IDX(_n, _prop, _i),
#define CI_LD_SEQ(_n)                                                                              \
    {                                                                                              \
        .name = DT_NODE_FULL_NAME(_n), .len = DT_PROP_LEN(_n, sequence),                           \
        .codes = (const uint32_t[]){DT_FOREACH_PROP_ELEM(_n, sequence, CI_LD_CODE)},               \
    },
#define CI_LD_IGN(_n)                                                                              \
    COND_CODE_1(DT_NODE_HAS_PROP(_n, ignore_keys),                                                 \
                ((const uint32_t[]){DT_FOREACH_PROP_ELEM(_n, ignore_keys, CI_LD_CODE) 0}),         \
                ((const uint32_t[]){0}))

/* pass 1: one array of sequences per leader behavior */
#define CI_LEADER(_label, _color, _done, _abort)                                                   \
    static struct ci_seq lseq_##_label[] = {                                                       \
        DT_FOREACH_CHILD_STATUS_OKAY(DT_NODELABEL(_label), CI_LD_SEQ)};
CI_LEADERS

/* pass 2: the table of leaders */
#undef CI_LEADER
#define CI_LEADER(_label, _color, _done, _abort)                                                   \
    {                                                                                              \
        .dev = DEVICE_DT_NAME(DT_NODELABEL(_label)), .seqs = lseq_##_label,                        \
        .nseq = ARRAY_SIZE(lseq_##_label), .done_fx = (_done), .abort_fx = (_abort),               \
        .ignore = CI_LD_IGN(DT_NODELABEL(_label)),                                                 \
        .n_ignore = DT_PROP_LEN_OR(DT_NODELABEL(_label), ignore_keys, 0),                          \
        .timeout_ms = DT_PROP_OR(DT_NODELABEL(_label), timeout_ms, 0),                             \
    },
static struct ci_leader leaders[] = {CI_LEADERS{.dev = NULL}};
#define CI_LEADER_COUNT (ARRAY_SIZE(leaders) - 1)

#define CI_LEADER_SEQ(_name, _color, _fx) {#_name, (_color), (_fx)},
static const struct ci_seqcfg seqcfgs[] = {CI_LEADER_SEQS{NULL, 0, 0}};
#define CI_SEQCFG_COUNT (ARRAY_SIZE(seqcfgs) - 1)

/* palette: entry i+1 of the lit-key map -> colour. Leader defaults first, then per sequence. */
#undef CI_LEADER
#define CI_LEADER(_label, _color, _done, _abort) (_color),
#undef CI_LEADER_SEQ
#define CI_LEADER_SEQ(_name, _color, _fx) (_color),
static const uint32_t ld_palette[] = {CI_LEADERS CI_LEADER_SEQS 0};

/* ------------------------------------------------------------------------- */
/* Tunables with defaults                                                    */
/* ------------------------------------------------------------------------- */

#ifndef CI_BREATHE_MS
#define CI_BREATHE_MS 2400
#endif
#ifndef CI_BLINK_MS
#define CI_BLINK_MS 700
#endif
#ifndef CI_LEADER_BRIGHTNESS_PERCENT
#define CI_LEADER_BRIGHTNESS_PERCENT CI_BRIGHTNESS_PERCENT
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
static uint32_t st_held; /* bit per CI_HELD rule, in order of appearance */
static uint8_t splash_idx;
static uint8_t ld_pal[CI_POSITIONS]; /* leader lights: palette entry per key position, 0 = off */
static bool ld_any;
static int64_t ld_stamp; /* peripheral: when the central last told us about the leader */

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

static uint8_t anim_factor_p(uint8_t anim, int64_t now, uint32_t period) {
    uint32_t breathe_ms = period ? period : CI_BREATHE_MS;
    uint32_t blink_ms = period ? period : CI_BLINK_MS;

    switch (anim) {
    case CI_ANIM_BREATHE: {
        uint32_t p = now % breathe_ms;
        uint32_t half = breathe_ms / 2;
        uint32_t v = p < half ? (p * 255) / half : ((breathe_ms - p) * 255) / half;
        v = (v * v) / 255; /* ease */
        return 15 + (v * 240) / 255;
    }
    case CI_ANIM_BLINK:
        return (now % blink_ms) < (blink_ms / 2) ? 255 : 0;
    default:
        return 255;
    }
}

static uint8_t anim_factor(uint8_t anim, int64_t now) {
    return anim_factor_p(anim, now, 0);
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

#include "keyfx_fx_motion.h"

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
        return fx_motion_level(d, f, led, t);
    }
}

/* colour of an effect at an LED: plain, rainbow, or a left-to-right gradient */
static uint32_t fx_color(uint32_t c, uint32_t c2, int led, int32_t t) {
    if (c == CI_RAINBOW) {
        struct led_rgb h = hue_rgb((lx[led] * 2 + t / 5) % 360);

        return ((uint32_t)h.r << 16) | ((uint32_t)h.g << 8) | h.b;
    }
    if (c2) {
        int32_t span = MAX(lx_max - lx_min, 1);
        uint32_t g = (uint32_t)(((lx[led] - lx_min) * 256) / span);
        uint32_t out = 0;

        g = MIN(g, 256u);
        for (int sh = 16; sh >= 0; sh -= 8) {
            uint32_t x = (c >> sh) & 0xFF, y = (c2 >> sh) & 0xFF;

            out |= ((x * (256 - g) + y * g) >> 8) << sh;
        }
        return out;
    }
    return c;
}

/* blend an effect colour towards the head colour (matrix) by m/255 */
static uint32_t fx_head(uint32_t c, uint32_t head, uint32_t m) {
    uint32_t out = 0;

    if (!head || !m) {
        return c;
    }
    for (int sh = 16; sh >= 0; sh -= 8) {
        uint32_t x = (c >> sh) & 0xFF, y = (head >> sh) & 0xFF;

        out |= ((x * (255 - m) + y * m) / 255) << sh;
    }
    return out;
}

static void fx_add(int led, uint32_t lvl, uint32_t color, uint32_t pct) {
    struct led_rgb add = rgb_from(color, lvl, pct);

    out_frame[led].r = MIN(255, out_frame[led].r + add.r);
    out_frame[led].g = MIN(255, out_frame[led].g + add.g);
    out_frame[led].b = MIN(255, out_frame[led].b + add.b);
}

/* How far into the effect itself a part is, `tp` ms after the part started: a speed that can
 * change steadily (accel), so the effect can speed up or slow down. */
static int32_t part_speed(const struct ci_part *p, int32_t ref_dur) {
    return p->duration_ms ? (ref_dur * 100) / p->duration_ms : (p->speed ? p->speed : 100);
}

static int32_t part_warp(const struct ci_part *p, int32_t ref_dur, int32_t tp) {
    int64_t sp = part_speed(p, ref_dur);
    int64_t u = sp * tp / 100 + (int64_t)p->accel * tp * tp / 200000;

    return u < 0 ? 0 : (int32_t)MIN(u, (int64_t)ref_dur);
}

static uint16_t part_len(const struct ci_part *p, int32_t ref_dur) {
    int32_t sp = part_speed(p, ref_dur);

    if (ref_dur <= 0 || sp <= 0) {
        return 0;
    }
    for (int32_t tp = 0; tp < 30000; tp += 10) {
        bool stalled = sp + (int32_t)p->accel * tp / 1000 <= 0; /* slowed to a stop */

        if (stalled || part_warp(p, ref_dur, tp) >= ref_dur) {
            return (uint16_t)tp;
        }
    }
    return 30000;
}

static void mixes_init(void) {
    lx_min = lx_max = lx[0];
    ly_min = ly_max = ly[0];
    for (int i = 0; i < CI_N; i++) {
        lx_min = MIN(lx_min, lx[i]);
        lx_max = MAX(lx_max, lx[i]);
        ly_min = MIN(ly_min, ly[i]);
        ly_max = MAX(ly_max, ly[i]);
    }
    for (int m = 0; m < FX_COUNT - FX_BASE_COUNT; m++) {
        uint32_t total = 0;

        for (int i = 0; i < mixes[m].n; i++) {
            struct ci_part *p = &mixes[m].parts[i];

            p->len = p->fx < FX_BASE_COUNT ? part_len(p, fx_defs[p->fx].duration_ms) : 0;
            uint32_t rep = p->repeat ? p->repeat : 1;
            uint32_t end = p->start_ms + rep * p->len + (rep - 1) * p->pause_ms;

            total = MAX(total, end);
        }
        fx_defs[FX_BASE_COUNT + m].duration_ms = (uint16_t)MIN(total + 30, 65535u);
    }
}

static void paint_mix(const struct ci_fx_def *d, const struct fx_inst *f, int32_t t) {
    const struct ci_mix *m = &mixes[d->arg];

    for (int i = 0; i < m->n; i++) {
        const struct ci_part *p = &m->parts[i];
        int32_t tp = t - p->start_ms;
        uint32_t cyc = p->len + p->pause_ms;
        uint32_t rep = p->repeat ? p->repeat : 1;

        if (!p->len || tp < 0 || (uint32_t)tp / cyc >= rep) {
            continue;
        }
        tp = (uint32_t)tp % cyc;
        if (tp > p->len) {
            continue;
        }
        const struct ci_fx_def *rd = &fx_defs[p->fx];
        struct fx_inst pf = *f;

        pf.seed ^= (uint32_t)(i + 1) * 0x9E3779B1u;
        if (p->origin) {
            pos_xy(p->origin - 1, &pf.ox, &pf.oy);
        }
        int32_t u = part_warp(p, rd->duration_ms, tp);
        uint32_t env = 255;

        if (p->fade_in_ms && tp < p->fade_in_ms) {
            env = (env * tp) / p->fade_in_ms;
        }
        if (p->fade_out_ms && (int32_t)p->len - tp < p->fade_out_ms) {
            env = (env * ((int32_t)p->len - tp)) / p->fade_out_ms;
        }
        if (p->pulse_ms >= 2) {
            env = (tp % p->pulse_ms) < (p->pulse_ms / 2) ? env : 0;
        }
        if (p->breathe_ms >= 2) {
            env = (env * anim_factor_p(CI_ANIM_BREATHE, tp, p->breathe_ms)) / 255;
        }
        uint32_t pct = (CI_FX_BRIGHTNESS_PERCENT * (p->brightness ? p->brightness : 100)) / 100;
        uint32_t color = p->color ? p->color : rd->color;

        for (int led = 0; led < CI_N; led++) {
            fx_head_mix = 0;
            uint32_t lvl = (fx_level(rd, &pf, led, u) * env) / 255;

            if (lvl) {
                fx_add(led, lvl, fx_head(fx_color(color, p->color2, led, u), rd->head, fx_head_mix), pct);
            }
        }
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

        if (d->type == CI_FX_MIX) {
            paint_mix(d, f, t);
            continue;
        }
        for (int led = 0; led < CI_N; led++) {
            fx_head_mix = 0;
            uint32_t lvl = fx_level(d, f, led, t);

            if (lvl) {
                fx_add(led, lvl, fx_head(fx_color(d->color, 0, led, t), d->head, fx_head_mix),
                       CI_FX_BRIGHTNESS_PERCENT);
            }
        }
    }
    return running;
}

/* ------------------------------------------------------------------------- */
/* Indicator rules                                                           */
/* ------------------------------------------------------------------------- */

static bool rule_active(const struct ci_rule *r, int held_idx) {
    uint8_t profile = (st_flags >> ST_PROFILE_SHIFT) & 0x7;

    if (r->when != CI_ALWAYS) {
        if (r->when & 0x40) { /* CI_ONLY: that layer must be the top one */
            if (!st_layers || (31 - __builtin_clz((unsigned)st_layers)) != (r->when & 7)) {
                return false;
            }
        } else if (!(st_layers & BIT(r->when & 7))) {
            return false;
        }
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
    case CI_HELD:
        return held_idx < 24 && (st_held & BIT(held_idx));
    default:
        return false;
    }
}

/* colour of the i-th of n keys of a rule: one colour, a gradient through up to three colours,
 * or a rainbow along the keys */
static uint32_t rule_color(const struct ci_rule *r, int i, int n, int64_t now) {
    if (r->color == CI_RAINBOW) {
        struct led_rgb h = hue_rgb((uint16_t)((i * 360 / MAX(n, 1) + now / 12) % 360));

        return ((uint32_t)h.r << 16) | ((uint32_t)h.g << 8) | h.b;
    }
    if (!r->color2) {
        return r->color;
    }
    uint32_t c[3] = {r->color, r->color2, r->color3};
    int nc = r->color3 ? 3 : 2;
    uint32_t t = n > 1 ? ((uint32_t)i * 256 * (nc - 1)) / (n - 1) : 0;
    int seg = MIN((int)(t >> 8), nc - 2);
    uint32_t f = t - ((uint32_t)seg << 8);
    uint32_t a = c[seg], b = c[seg + 1], out = 0;

    f = MIN(f, 256u);
    for (int sh = 16; sh >= 0; sh -= 8) {
        uint32_t x = (a >> sh) & 0xFF, y = (b >> sh) & 0xFF;

        out |= ((x * (256 - f) + y * f) >> 8) << sh;
    }
    return out;
}

static uint8_t rule_level[ARRAY_SIZE(rules)];
static int64_t rule_t[ARRAY_SIZE(rules)];

/* Paints indicator rules onto out_frame; returns true while anything animates or fades. */
static bool paint_rules(int64_t now, bool *any_lit) {
    bool animated = false;

    *any_lit = false;
    if (!(st_flags & ST_IND_ON)) {
        return false;
    }

    int held_idx = -1;
    size_t ri = 0;

    for (const struct ci_rule *r = rules; r->src != CI_END; r++, ri++) {
        if (r->src == CI_HELD) {
            held_idx++;
        }
        bool on = rule_active(r, held_idx);
        uint8_t target = on ? 255 : 0;
        /* frames are only drawn while something moves, so a long gap means "one frame ago" */
        int64_t dt = rule_t[ri] ? MIN(now - rule_t[ri], (int64_t)(CI_FRAME_MS * 2)) : CI_FRAME_MS;

        rule_t[ri] = now;
        if (rule_level[ri] != target) {
            uint16_t ms = on ? r->fade_in_ms : r->fade_out_ms;

            if (!ms) {
                rule_level[ri] = target;
            } else {
                int32_t step = (int32_t)(dt * 255 / ms) + 1;

                rule_level[ri] = on ? MIN(255, rule_level[ri] + step)
                                    : (rule_level[ri] > step ? rule_level[ri] - step : 0);
                animated = true;
            }
        }
        if (!rule_level[ri]) {
            continue;
        }
        uint8_t factor = ((uint32_t)anim_factor_p(r->anim, now, r->period_ms) * rule_level[ri]) / 255;
        uint8_t pct = r->brightness ? r->brightness : CI_BRIGHTNESS_PERCENT;

        if (r->anim != CI_ANIM_SOLID || r->color == CI_RAINBOW) {
            animated = true;
        }
        for (uint8_t i = 0; i < r->n; i++) {
            int led = pos_led(r->pos[i]);

            if (led >= 0) {
                out_frame[led] = rgb_from(rule_color(r, i, r->n, now), factor, pct);
                *any_lit = true;
            }
        }
    }
    return animated;
}

/* Leader key lights (what to light was decided by the central half). */
static bool paint_leader(int64_t now) {
    if (!ld_any) {
        return false;
    }
#if !CI_IS_CENTRAL
    if (now - ld_stamp > 6000) { /* the central went quiet: do not stay lit forever */
        memset(ld_pal, 0, sizeof(ld_pal));
        ld_any = false;
        return false;
    }
#endif
    uint8_t factor = anim_factor(CI_LEADER_ANIM, now);

    for (uint8_t p = 0; p < CI_POSITIONS; p++) {
        int led = pos_led(p);

        if (led >= 0 && ld_pal[p] && ld_pal[p] <= ARRAY_SIZE(ld_palette) - 1) {
            out_frame[led] = rgb_from(ld_palette[ld_pal[p] - 1], factor,
                                      CI_LEADER_BRIGHTNESS_PERCENT);
        }
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* Frame output                                                              */
/* ------------------------------------------------------------------------- */

/* Caller holds ov_lock. Returns true while animation needs more frames. */
static bool push_frame(int *rc_out) {
    int64_t now = k_uptime_get();
    bool any_lit;

    memcpy(out_frame, last_frame, sizeof(out_frame));
    bool dim = CI_LEADER_DIM_OTHERS && ld_any;
    bool animated = false;

    if (dim) { /* a leader sequence is running: only its keys stay lit */
        memset(out_frame, 0, sizeof(out_frame));
    } else {
        animated = paint_rules(now, &any_lit);
    }
    animated |= paint_leader(now);
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

#if !CI_IS_CENTRAL
/* The central sends the right half's lit keys as 4-bit palette entries, 14 per message,
 * in the order the right-half keys appear in CI_KEYS. */
static void ld_apply(int base, uint32_t lo24, uint32_t hi32) {
    int r = 0;

    for (uint8_t p = 0; p < CI_POSITIONS; p++) {
        if (geo_keys[p].half != CI_RIGHT) {
            continue;
        }
        int n = r - base;

        if (n >= 0 && n < 14) {
            ld_pal[p] = n < 6 ? (lo24 >> (4 * n)) & 0xF : (hi32 >> (4 * (n - 6))) & 0xF;
        }
        r++;
    }
    ld_any = false;
    for (uint8_t p = 0; p < CI_POSITIONS; p++) {
        ld_any |= ld_pal[p] != 0;
    }
    ld_stamp = k_uptime_get();
}
#endif

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
        st_held = p2 >> 8;
        kick();
    } else if (op == KEYFX_OP_LEADER_A || op == KEYFX_OP_LEADER_B) {
        ld_apply(op == KEYFX_OP_LEADER_B ? 14 : 0, p1 & 0xFFFFFF, p2);
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
    mixes_init();
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

static bool key_down[CI_POSITIONS];
static int64_t last_press[CI_POSITIONS];

static void held_poll(void);
static void ld_resend(void);
static void ld_relight(void);

static void send_state(void) {
    keyfx_sync_send(((uint32_t)KEYFX_OP_STATE << 24) | ((uint32_t)st_flags << 16) |
                       ((uint32_t)st_mods << 8) | st_layers,
                   (st_held << 8) | hid_ind);
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

    uint32_t held = 0;
    int hi = 0;
    bool waiting = false;
    int64_t now = k_uptime_get();

    for (const struct ci_rule *r = rules; r->src != CI_END; r++) {
        if (r->src != CI_HELD) {
            continue;
        }
        if (hi < 24 && r->hold_pos < CI_POSITIONS && key_down[r->hold_pos]) {
            if (now - last_press[r->hold_pos] >= r->hold_ms) {
                held |= BIT(hi);
            } else {
                waiting = true;
            }
        }
        hi++;
    }
    if (waiting) {
        held_poll();
    }

    bool changed = layers != st_layers || mods != st_mods || flags != st_flags || held != st_held;
    st_held = held;

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

static void held_poll(void) {
    k_work_reschedule(&state_work, K_MSEC(40));
}

static void beat_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(beat_work, beat_fn);

static void beat_fn(struct k_work *work) {
    /* periodic resend heals a peripheral that reconnected or rebooted */
    check_state(true);
    ld_resend();
    k_work_reschedule(&beat_work, K_SECONDS(2));
}

static void schedule_state_check(void) {
    k_work_schedule(&state_work, K_MSEC(10));
}

static int on_state_event(const zmk_event_t *eh) {
    if (as_zmk_layer_state_changed(eh)) {
        ld_relight();
    }
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

static void ld_on_binding(const char *dev, uint32_t position);
static void ld_on_position(uint8_t pos);

void keyfx_on_binding(const struct zmk_behavior_binding *b,
                     const struct zmk_behavior_binding_event *ev) {
    static const char kfx_name[] = DEVICE_DT_NAME(DT_NODELABEL(kfx));

    if (!b->behavior_dev || strcmp(b->behavior_dev, kfx_name) == 0) {
        return; /* our own sync messages */
    }
    ld_on_binding(b->behavior_dev, ev->position);

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

    ld_on_position(p);

    last_press[p] = now;
    key_down[p] = true;

    for (const struct ci_rule *r = rules; r->src != CI_END; r++) {
        if (r->src == CI_HELD && r->hold_pos == p) {
            held_poll();
            break;
        }
    }

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

/* ---- central: leader key lighting ---- */


struct ld_ev {
    uint8_t kind; /* 0 = a leader behavior started (arg = leader index), 1 = key typed */
    uint32_t arg;
};
K_MSGQ_DEFINE(ld_q, sizeof(struct ld_ev), 16, 4);

static volatile bool ld_armed;
static int8_t ld_cur = -1;
static uint8_t ld_depth;
static uint32_t ld_alive;
static uint8_t ld_last_pos;

static bool name_eq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char x = (*a == '-') ? '_' : (*a | 0x20);
        char y = (*b == '-') ? '_' : (*b | 0x20);

        if (x != y) {
            return false;
        }
    }
    return *a == *b;
}

static void ld_init(void) {
    for (size_t k = 0; k < CI_LEADER_COUNT; k++) {
        for (uint8_t i = 0; i < leaders[k].nseq; i++) {
            struct ci_seq *q = &leaders[k].seqs[i];

            q->pal = MIN(k + 1, CI_LD_MAX_PAL);
            q->fx = leaders[k].done_fx;
            for (size_t j = 0; j < CI_SEQCFG_COUNT; j++) {
                if (name_eq(seqcfgs[j].name, q->name)) {
                    size_t pal = CI_LEADER_COUNT + j + 1;

                    q->pal = pal <= CI_LD_MAX_PAL ? pal : q->pal;
                    if (seqcfgs[j].fx != FX_NONE) {
                        q->fx = seqcfgs[j].fx;
                    }
                }
            }
        }
    }
}

/* The leader code at a key position: from the highest active layer that lists the position
 * (CI_LEADER_KEYS). 0 = the position is not listed. */
static uint32_t ld_code_at(uint8_t pos) {
    int best = -1;
    uint32_t code = 0;

    for (const struct ci_lkl *g = lkeys; g->keys; g++) {
        if (g->layer >= 8 || !zmk_keymap_layer_active(g->layer) || (int)g->layer < best) {
            continue;
        }
        for (uint8_t i = 0; i < g->n; i++) {
            if (g->keys[i].pos == pos) {
                best = g->layer;
                code = g->keys[i].code;
            }
        }
    }
    return code;
}

static void ld_send(void) {
    uint32_t nib[2] = {0, 0}; /* 14 nibbles per message: 6 in the low 24 bits, 8 in p2 */
    uint32_t lo[2] = {0, 0}, hi[2] = {0, 0};
    int r = 0;

    for (uint8_t p = 0; p < CI_POSITIONS; p++) {
        if (geo_keys[p].half != CI_RIGHT) {
            continue;
        }
        int bank = r / 14, n = r % 14;

        if (bank < 2) {
            if (n < 6) {
                lo[bank] |= (uint32_t)ld_pal[p] << (4 * n);
            } else {
                hi[bank] |= (uint32_t)ld_pal[p] << (4 * (n - 6));
            }
        }
        r++;
    }
    ARG_UNUSED(nib);
    keyfx_sync_send(((uint32_t)KEYFX_OP_LEADER_A << 24) | lo[0], hi[0]);
    keyfx_sync_send(((uint32_t)KEYFX_OP_LEADER_B << 24) | lo[1], hi[1]);
}

static void ld_resend(void) {
    if (ld_cur >= 0) {
        ld_send();
    }
}

/* Work out which keys to light from the sequences that are still possible. */
static void ld_rebuild(void) {
    uint8_t pal[CI_POSITIONS] = {0};
    bool any = false;

    if (ld_cur >= 0) {
        const struct ci_leader *ldr = &leaders[ld_cur];

        for (uint8_t p = 0; p < CI_POSITIONS; p++) {
            uint32_t c = ld_code_at(p);

            for (uint8_t sq = 0; c && sq < ldr->nseq && !pal[p]; sq++) {
                if (!(ld_alive & BIT(sq))) {
                    continue;
                }
                for (uint8_t j = ld_depth; j < ldr->seqs[sq].len; j++) {
                    if ((ldr->seqs[sq].codes[j] & 0xFFFFFF) == (c & 0xFFFFFF)) {
                        pal[p] = ldr->seqs[sq].pal;
                        any = true;
                        break;
                    }
                }
            }
        }
    }
    k_mutex_lock(&ov_lock, K_FOREVER);
    memcpy(ld_pal, pal, sizeof(ld_pal));
    ld_any = any;
    k_mutex_unlock(&ov_lock);
    kick();
}

static void ld_timeout_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ld_timeout_work, ld_timeout_fn);

static void ld_end(int fx, uint8_t origin) {
    ld_cur = -1;
    ld_armed = false;
    k_work_cancel_delayable(&ld_timeout_work);
    ld_rebuild();
    ld_send();
    if (fx != FX_NONE && fx < FX_COUNT) {
        trigger_fx(fx, origin);
    }
}

static void ld_arm_timeout(void) {
    uint16_t ms = ld_cur >= 0 && leaders[ld_cur].timeout_ms ? leaders[ld_cur].timeout_ms
                                                              : CI_LEADER_TIMEOUT_MS;

    if (ms) {
        k_work_reschedule(&ld_timeout_work, K_MSEC(ms));
    }
}

static void ld_timeout_fn(struct k_work *work) {
    if (ld_cur >= 0) {
        ld_end(leaders[ld_cur].abort_fx, ld_last_pos);
    }
}

static void ld_key(uint8_t pos, uint32_t code) {
    const struct ci_leader *ldr = &leaders[ld_cur];

    if (!code && !CI_LEADER_STRICT) {
        return; /* not a key we know: leave it to the leader module, keep the lights */
    }
    ld_last_pos = pos;
    if (code && (code >> 16) == 0x07 && (code & 0xFFFF) >= 0xE0 && (code & 0xFFFF) <= 0xE7) {
        return; /* a bare modifier never ends or advances a sequence */
    }
    for (uint8_t i = 0; code && i < ldr->n_ignore; i++) {
        if ((ldr->ignore[i] & 0xFFFFFF) == (code & 0xFFFFFF)) {
            return;
        }
    }
    uint32_t alive = 0;

    for (uint8_t s = 0; s < ldr->nseq && s < CI_LD_MAX_SEQ; s++) {
        if ((ld_alive & BIT(s)) && ldr->seqs[s].len > ld_depth &&
            (ldr->seqs[s].codes[ld_depth] & 0xFFFFFF) == (code & 0xFFFFFF)) {
            alive |= BIT(s);
        }
    }
    if (!alive) {
        ld_end(ldr->abort_fx, ld_last_pos);
        return;
    }
    ld_depth++;
    ld_alive = alive;
    for (uint8_t s = 0; s < ldr->nseq; s++) {
        if ((alive & BIT(s)) && ldr->seqs[s].len == ld_depth) {
            ld_end(ldr->seqs[s].fx, ld_last_pos);
            return;
        }
    }
    ld_rebuild();
    ld_send();
    ld_arm_timeout();
}

static void ld_fn(struct k_work *work) {
    struct ld_ev e;

    while (k_msgq_get(&ld_q, &e, K_NO_WAIT) == 0) {
        if (e.kind == 0) {
            ld_cur = e.arg;
            ld_depth = 0;
            ld_alive = leaders[ld_cur].nseq >= 32 ? 0xFFFFFFFFu : BIT(leaders[ld_cur].nseq) - 1;
            ld_last_pos = 0;
            ld_rebuild();
            ld_send();
            ld_arm_timeout();
        } else if (e.kind == 2) { /* layers changed: show the keys of the new layer */
            if (ld_cur >= 0) {
                ld_rebuild();
                ld_send();
            }
        } else if (ld_cur >= 0) {
            ld_key(e.arg & 0xFF, ld_code_at(e.arg & 0xFF) & 0xFFFFFF);
        }
    }
}
/* Handled a few ms late, so ZMK has finished deciding hold-taps and layer changes first. */
static K_WORK_DELAYABLE_DEFINE(ld_work, ld_fn);
#define LD_DELAY K_MSEC(8)

/* The key that started the leader (a hold-tap or tap dance) must not count as the first key
 * of the sequence, even if its press is seen just after the leader started. */
static uint8_t ld_skip_pos = 0xFF;
static int64_t ld_skip_until;

static void ld_on_binding(const char *dev, uint32_t position) {
    for (size_t k = 0; k < CI_LEADER_COUNT; k++) {
        if (strcmp(leaders[k].dev, dev) == 0) {
            struct ld_ev e = {.kind = 0, .arg = k};

            ld_skip_pos = position < CI_POSITIONS ? (uint8_t)position : 0xFF;
            ld_skip_until = k_uptime_get() + 120;
            ld_armed = true;
            if (k_msgq_put(&ld_q, &e, K_NO_WAIT) == 0) {
                k_work_schedule(&ld_work, LD_DELAY);
            }
#ifdef CI_LEADER_DEBUG_FX
            /* test aid: play this effect whenever a leader behavior start is detected */
            struct pending_fx dbg = {.fx = CI_LEADER_DEBUG_FX, .origin = 0};

            if (k_msgq_put(&act_q, &dbg, K_NO_WAIT) == 0) {
                k_work_submit(&act_work);
            }
#endif
            return;
        }
    }
}

/* Called for every key press (position) while a leader sequence is running. The leader
 * module swallows the keycodes, but ZMK still tells us which key position was pressed. */
static void ld_relight(void) {
    if (ld_armed) {
        struct ld_ev e = {.kind = 2, .arg = 0};

        if (k_msgq_put(&ld_q, &e, K_NO_WAIT) == 0) {
            k_work_schedule(&ld_work, LD_DELAY);
        }
    }
}

static void ld_on_position(uint8_t pos) {
    for (size_t i = 0; i < ARRAY_SIZE(ld_ignore_pos) - 1; i++) {
        if (ld_ignore_pos[i] == pos) {
            return; /* a layer key: it changes what is lit, it does not end the sequence */
        }
    }
    if (ld_armed && pos == ld_skip_pos && k_uptime_get() < ld_skip_until) {
        return;
    }
    if (ld_armed) {
        struct ld_ev e = {.kind = 1, .arg = pos};

        if (k_msgq_put(&ld_q, &e, K_NO_WAIT) == 0) {
            k_work_schedule(&ld_work, LD_DELAY);
        }
    }
}

static int keyfx_central_init(void) {
    ld_init();
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
