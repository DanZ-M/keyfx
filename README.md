# keyfx — per-key lighting for ZMK

keyfx turns a WS2812 LED strip under your keys into a status display and an effects engine.
It is a ZMK module: your `.keymap` stays as standard as it was, and every colour and effect
is set in one file, `config/keyfx_config.h`.

**What it can do**

| Feature | What you see |
|---|---|
| Indicators | Caps Lock, Num Lock off, active layer, modifiers (incl. sticky), Caps Word, Bluetooth profile |
| Effects | ripple, wave, rain, bloom, flash, keystroke splash, plus laser beams, snake, spiral, bouncing ball, lightning, swipe and Matrix rain |
| Effects tied to real actions | a combo, macro, tap dance or key binding plays an effect, no keymap edits |
| Held-key lights | hold a key and other keys light up (optionally after a delay) |
| Leader-key lights | press the leader key and every key of every sequence lights; the lights narrow as you type |
| Split keyboards | both halves show the same thing; the left (central) half decides |

---

## 1. Requirements

- A ZMK keyboard with one WS2812 strip on each half (SPI driver), e.g. nice!nano.
- ZMK on Zephyr 4.x, built with GitHub Actions or locally.
- Underglow enabled (`CONFIG_ZMK_RGB_UNDERGLOW=y`). keyfx sits in front of it.
- Leader lights only: [urob/zmk-leader-key](https://github.com/urob/zmk-leader-key).

---

## 2. Install (5 steps)

### Step 1 — add the module to `config/west.yml`

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: DanZ-M
      url-base: https://github.com/DanZ-M
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: keyfx
      remote: DanZ-M
      revision: main
  self:
    path: config
```

Keep any other projects you already have (for example the leader module).

### Step 2 — create the `&kfx` behavior in your shield

In your shield's `.dtsi` (see `your_keyboard_name.dtsi` for a full example):

```dts
/ {
    behaviors {
        kfx: keyfx {
            compatible = "zmk,behavior-keyfx";
            #binding-cells = <2>;
        };
    };
};
```

The label must be exactly `kfx`.

### Step 3 — describe the LED strip in the overlay

In the `.overlay` of each half (see `example_nice_nano_nrf52840_zmk.overlay`):

```dts
#include <dt-bindings/led/led.h>

/ {
    chosen {
        zmk,underglow = &led_overlay;
    };

    led_overlay: led_overlay {
        compatible = "keyfx,led-overlay";
        strip = <&led_strip>;      /* your real WS2812 node */
        chain-length = <27>;       /* LEDs on this half */
    };
};
```

`chain-length` is the number of LEDs on **that** half. The real strip node keeps its own
`chain-length`.

### Step 4 — turn it on and copy the config

In your shield's `.conf`:

```
CONFIG_ZMK_RGB_UNDERGLOW=y
CONFIG_KEYFX=y   # optional, it is on by default
```

Copy `keyfx_config.h` from this repository to your keyboard repo's `config/` folder, next to
the `.keymap`. This is the only file you edit from now on.

### Step 5 — (optional) control keys in the keymap

Add one include to the keymap to get the `KFX_` names, then use `&kfx` where you want a key:

```
#include <dt-bindings/zmk/keyfx.h>

&kfx KFX_SPLASH_TOG 0   /* keystroke splash on/off  */
&kfx KFX_SPLASH_NEXT 0  /* next splash style        */
&kfx KFX_IND_TOG 0      /* all indicators on/off    */
```

Nothing else in the keymap changes. If you already include `korz_fx.h`, it still works.

---

## 3. Editing `keyfx_config.h`

The file is split into numbered sections. Everything is commented in place.

| # | Section | Purpose |
|---|---|---|
| 1 | General settings | brightness, breathing/blink speed, colours |
| 2 | Geometry | which LED sits under which key position (`CI_KEYS`) and the extra LEDs |
| 3 | Indicator rules | `CI_RULE(...)` and `CI_HELD_RULE(...)` |
| 4 | Effects | named effects (`CI_EFFECTS`): classic ones and the motion effects below |
| 5 | Effect mixer | `CI_MIXES`: combine effects |
| 6 | Splash styles | what a keystroke splash looks like |
| 7 | Actions | `CI_ACTIONS`: combo/macro/binding → effect |
| 8 | Leader key lights | `CI_LEADERS`, `CI_LEADER_SEQS`, `CI_LEADER_KEYS` |
| 9 | Position triggers | `CI_TRIGGERS` (older, prefer section 7) |

### Indicator rule

```
CI_RULE(source, arg, colour, animation, layer, key positions...)
```

```
CI_RULE(CI_CAPS_ON, 0, CI_RED, CI_ANIM_SOLID, CI_ALWAYS, 36, 19)
CI_RULE(CI_LAYER,   1, CI_YELLOW, CI_ANIM_BREATHE, CI_ALWAYS, 2, 3, 4)
```

Sources: `CI_CAPS_ON`, `CI_NUM_OFF`, `CI_NUM_ON`, `CI_LAYER`, `CI_MOD`, `CI_CAPS_WORD`,
`CI_BT_CONNECTED`, `CI_BT_SEARCHING`, `CI_SPLASH_ON`.
Animations: `CI_ANIM_SOLID`, `CI_ANIM_BREATHE`, `CI_ANIM_BLINK`.
The layer field limits the rule:
- `CI_ALWAYS`: every layer.
- a number `n`: lit whenever layer `n` is active. The base layer is always active, so a rule for layer 0 lights on every layer.
- `CI_ONLY(n)`: lit only while layer `n` is the highest active layer. Use it to put the same indicator on different keys on each layer.

### Per-rule settings, fades and gradients

`CI_RULEX` is `CI_RULE` plus options, each only for that rule:

```
CI_RULEX(CI_LAYER, 1, CI_RED, CI_ANIM_BREATHE, CI_ALWAYS,
         CI_OPTS(.color2 = CI_BLUE, .period_ms = 3000, .fade_in_ms = 400, .fade_out_ms = 600, .brightness = 40),
         2, 3, 4, 14, 15, 16)
```

| Option | Meaning |
|---|---|
| `.brightness` | percent, for this rule only |
| `.period_ms` | length of one breathe or blink |
| `.fade_in_ms`, `.fade_out_ms` | time to come up and to fade away |
| `.color2`, `.color3` | more colours: the listed keys blend from `colour` to `.color2` (to `.color3`), in the order you list them |

Use `CI_RAINBOW` as the colour for a moving rainbow along the keys. Options you leave out use the general settings.

### Motion effects (beam, snake, spiral, ball, lightning, swipe, matrix)

Written with `CI_EFFECTX(NAME, type, colour, duration_ms, .option = value, ...)` inside `CI_EFFECTS`. All options are optional. Directions are degrees (0 right, 90 down, 180 left, 270 up); distances and sizes are tenths of a key pitch; keys are written `CI_AT(position)`.

| Type | What it does |
|---|---|
| `CI_FX_BEAM` | laser shot(s) from a key; `.count` + `.spread` make a fan (e.g. 3 shots over 90°); `.invert` converges back |
| `CI_FX_SNAKE` | a wiggling trail from `.from` to `.to` |
| `CI_FX_SPIRAL` | a trail spiralling out from a key; `.invert` spirals in |
| `CI_FX_BALL` | a ball dropped from a key that bounces away |
| `CI_FX_LIGHTNING` | a jagged bolt with random brightness and flicker; `.random` picks a new direction each time |
| `CI_FX_SWIPE` | a wall of light in any direction; `.bidir` spreads both ways, `.invert` contracts, `.fill` leaves the passed keys lit |
| `CI_FX_MATRIX` | falling columns with a shimmering shine; `.count` = % of columns |

| Option | Meaning |
|---|---|
| `.from`, `.to` | start key and end key (the end key sets direction and distance) |
| `.angle`, `.dist` | direction and distance when no `.to` is given |
| `.count`, `.spread` | number of shots / bolts / % of columns; degrees covered by a fan |
| `.size`, `.width` | trail length (% of the path) and thickness |
| `.wiggle`, `.turns`, `.height`, `.bounce` | snake/bolt sideways amount, spiral turns, ball drop height, ball bounciness (%) |
| `.accel` | -100..100: + starts slow and ends fast, - the opposite |
| `.invert`, `.random`, `.bidir`, `.fill` | 1 = on |

**Matrix options:** `.head` (colour of the leading light, e.g. `CI_WHITE` for a white wisp), `.count` and `.count_max` (fewest and most columns that rain, %; picked at random each time), `.speed_min` and `.speed_max` (each column falls at its own random speed, % of normal), `.dist` (trail length), `.flicker` (strength in %; grows toward the tail, each column at its own rate).

Without `.from` an effect starts at the key that triggered it. Motion effects can be used in mixes, actions, the leader lights and splash styles like any other effect.

**Snake:** one key wide, walks the key grid like the game (`.size` = length in keys, `.count` = steps, `.wiggle` = % chance to turn, `.to` = wander towards that key).

**Options for all motion effects:** `.margin` (keep N keys from the edges), `.loops` and `.pingpong` (repeat; every second loop backwards), `.breathe_ms`, `.nofade`, `.head` and `.color2` (trail colour → `.color2` → `.head`), `.center` (start in the middle) and `.aspect` (spiral width in % of its height).

### Bootloader light

`#define CI_BOOTLOADER_FX FX_BOOT` plays the `BOOT` effect (a looping, breathing orange–yellow–white spiral) on both halves just before a key bound to `&bootloader` reboots its half. Once a half is in bootloader mode the firmware is not running any more, so the animation cannot continue there; the last frame is kept. Does not apply to combos.

### Effect mixer

`CI_MIXES` builds a new effect out of the ones in section 4. Parts run together, or one after another
with `.start_ms`, and each has its own timing, look and speed:

```
#define CI_MIXES \
    CI_MIX(CELEBRATE, \
        CI_PART(KEY_RIPPLE, .fade_out_ms = 400) \
        CI_PART(UNLOCK, .start_ms = 500, .speed = 140, .accel = -30, .color = CI_CYAN, .color2 = CI_PINK) \
        CI_PART(KEY_BLOOM, .start_ms = 900, .pulse_ms = 200, .repeat = 2, .pause_ms = 200))
```

Use it like any effect, for example `CI_ON_COMBO_NAME(CELEBRATE, logoff)`.

| Option | Meaning |
|---|---|
| `.start_ms` | wait before the part starts (a pause, or a sequence) |
| `.duration_ms` | play the effect over this long instead of its own time |
| `.speed`, `.accel` | speed in percent; `.accel` is the change per second (+ faster, - slower) |
| `.brightness` | percent of the effect's brightness |
| `.fade_in_ms`, `.fade_out_ms` | fade the part in and out |
| `.pulse_ms`, `.breathe_ms` | flash or breathe while the part plays |
| `.repeat`, `.pause_ms` | repeat it, with a pause between |
| `.origin` | `CI_AT(position)`: start from another key |
| `.color`, `.color2` | change the colour; with both, a left-to-right gradient |

### Held-key lights

```
CI_HELD_RULE(held key, delay ms, colour, animation, layer, lit keys...)
```

```
CI_HELD_RULE(23, 0,   CI_CYAN,   CI_ANIM_SOLID,   CI_ALWAYS, 11)     /* hold 23: key 11 lights  */
CI_HELD_RULE(37, 300, CI_ORANGE, CI_ANIM_BREATHE, CI_ALWAYS, 3, 37)  /* hold 37 for 300 ms: 3 and 37 */
```

The held key lights only if you list it. The delay is checked about every 40 ms.
You can have up to 24 held rules.

### Effects that follow real actions

```
#define CI_ACTIONS \
    CI_ON_COMBO_NAME(LOGOUT, logoff) \
    CI_ON_BINDING(UNLOCK, kp, LC(LA(DEL)), 0)
```

`LOGOUT` and `UNLOCK` are effects from section 4. `logoff` is the combo's node name in your
keymap (lower case).

### Leader key lights

Set the label of your leader behavior and, if you like, a colour per sequence:

```
#define CI_LEADERS \
    CI_LEADER(leader, CI_WHITE, FX_NONE, FX_NONE)

#define CI_LEADER_SEQS \
    CI_LEADER_SEQ(tick, CI_GREEN, FX_NONE) \
    CI_LEADER_SEQ(usb,  CI_BLUE,  FX_NONE)
```

How it behaves:

1. You press the leader key: every other light goes off (indicators, underglow, back LEDs) and every key used by every sequence lights up, each in its sequence's colour. Set `CI_LEADER_DIM_OTHERS` to 0 to keep the other lights.
2. You type a key: lights that belong to other sequences go out, and the key you typed goes out too. The rest of the matching sequences stay lit.
3. Mistyped key, or a pause longer than the timeout: all lights go out (and the "aborted" effect plays, if set).
4. Sequence finished: the lights go out and that sequence's "done" effect plays, if set.

Sequence names come from your keymap (`tick`, `usb`, ...) and are not case sensitive.
The sequences themselves are read from the keymap, so you never write them twice.

**Tell keyfx where the keys are.** keyfx does not guess your layout. List, per layer, which
key position holds which letter or digit:

```
#define CI_LEADER_KEYS \
    CI_LKEYS(0,  /* base layer */ \
        CI_LKEY(Q, 1), CI_LKEY(W, 2), CI_LKEY(T, 5), CI_LKEY(LSHFT, 24)) \
    CI_LKEYS(3,  /* number layer */ \
        CI_LKEY(N1, 1), CI_LKEY(N2, 2))
```

- A key is looked up in the highest active layer that lists its position, so digits light only while their layer is active.
- Typing is followed by key position, so it works even though the leader module swallows the keys.
- Only listed keys are followed. A listed key that does not fit the sequence aborts the lights (list `ESC` to have a way to cancel). A key that is not listed is ignored, and the lights stay on, so layer keys and modifiers cannot break the effect. If such a key ends the sequence in the leader module itself, the lights go out at the timeout. Set `CI_LEADER_STRICT 1` if you prefer unlisted keys to abort.
- Replace the example positions with your own.
- To change layers in the middle of a sequence (for example to reach keypad digits), list your layer keys in `CI_LEADER_IGNORE_POS` (each position followed by a comma). They no longer abort the sequence, and the lit keys follow the new layer.

The timeout comes from the leader behavior's `timeout-ms`, or from `CI_LEADER_TIMEOUT_MS` (15 s by default).

---

## Dongle (a central without keys or LEDs)

keyfx also works when the keyboard has a **dongle**: a board with no keys that is the central, while the left and right halves are both peripherals. The dongle runs the keymap and keyfx works out what to show; each half draws it on its own LEDs.

What you need:

1. **The dongle's overlay** has a `keyfx,led-overlay` node **without** `strip` (it has no LEDs):
   ```dts
   led_overlay: led_overlay {
       compatible = "keyfx,led-overlay";
       chain-length = <27>;   /* any number, not used */
   };
   ```
   and the `kfx` behavior node, like on the halves. If your keymap has RGB keys (`&rgb_ug ...`) also set `zmk,underglow = &led_overlay;` in `chosen` and `CONFIG_ZMK_RGB_UNDERGLOW=y` on the dongle: the behavior must exist on the central for ZMK to forward the key to the halves; keyfx accepts and discards the frames.
2. **Each half** tells keyfx which half it is, so a half that is a peripheral of a dongle still lights the right keys. In the shield's `Kconfig.defconfig`:
   ```
   config KEYFX_HALF
       default 1    # left half (2 for the right half)
   ```
   With the value 0 (default) the central is the left half and the peripheral the right one, as in a normal two-part keyboard.
3. Build the halves as peripherals (`-DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`) and give the dongle `CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=2`, `CONFIG_BT_MAX_CONN=7`, `CONFIG_BT_MAX_PAIRED=7` (see ZMK's dongle guide).

Everything else (rules, effects, leader lights, bootloader light) works as before. Both halves now also agree on where an effect is, because the board's extent is computed from the whole geometry, not from the half that is running.

## 4. Updating

Change the `revision` of `keyfx` in `west.yml` (or leave `main`) and rebuild.
When you upload a new version to GitHub, files you deleted locally stay on GitHub, so remove them there by hand.

---

## 5. Troubleshooting

| Problem | Fix |
|---|---|
| Manifest error in the build | In `west.yml` the key is `url-base` (hyphen), not `url_base`. |
| `keyfx_config.h` not found | Copy it into your `config/` folder next to the `.keymap`. |
| `kfx` undeclared | Step 2 is missing, or the label is not exactly `kfx`. |
| No lights at all | `CONFIG_ZMK_RGB_UNDERGLOW=y` and `CONFIG_KEYFX=y`; the overlay must set `zmk,underglow = &led_overlay`. |
| Right half does not follow the left | Flash both halves with the same firmware build; the left must be the central. |
| Right half lights are stuck after the leader | It clears itself about 6 s after the central stops talking. |
| Effects from combos never play | Check the combo name in `CI_ACTIONS` is its lower-case node name. |
| Leader lights never appear | The leader label in `CI_LEADERS` must match your behavior's label, and `CONFIG_KEYFX_ACTION_HOOK` must stay on (it is by default). |
| Compile error about a key name | Do not name variables or sequences with a single letter such as `D`; ZMK defines those as keys. |

## Settings (Kconfig)

| Option | Default | Meaning |
|---|---|---|
| `CONFIG_KEYFX` | y (when underglow and the overlay exist) | Turn keyfx on |
| `CONFIG_KEYFX_ACTION_HOOK` | y (central) | Lets effects and leader lights follow real actions |
| `CONFIG_KEYFX_CAPS_WORD_PEEK` | y | Reads Caps Word state for the Caps Word indicator |
