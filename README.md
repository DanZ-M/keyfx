# keyfx — per-key lighting for ZMK

keyfx turns a WS2812 LED strip under your keys into a status display and an effects engine.
It is a ZMK module: your `.keymap` stays as standard as it was, and every colour and effect
is set in one file, `config/keyfx_config.h`.

**What it can do**

| Feature | What you see |
|---|---|
| Indicators | Caps Lock, Num Lock off, active layer, modifiers (incl. sticky), Caps Word, Bluetooth profile |
| Effects | ripple, wave, rain, bloom, flash, keystroke splash |
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
| 4 | Effects | named effects you can play (`CI_EFFECTS`) |
| 5 | Splash styles | what a keystroke splash looks like |
| 6 | Effects on actions | `CI_ACTIONS`: combo/macro/binding → effect |
| 7 | Leader key lights | `CI_LEADERS`, `CI_LEADER_SEQS` |
| 8 | Position triggers | `CI_TRIGGERS` (older, prefer section 6) |

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
`layer` limits the rule to when that layer is active; `CI_ALWAYS` means no limit.

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
- A key that is not listed aborts the sequence. List the modifiers you use inside sequences (such as `LSHFT`) so they do not.
- Replace the example positions with your own.

The timeout comes from the leader behavior's `timeout-ms`, or from `CI_LEADER_TIMEOUT_MS`.

---

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
