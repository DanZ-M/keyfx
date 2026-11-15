# Korz (hand-wired Corne clone) - ZMK user config

Split Corne-style keyboard, nice!nano v2 clone (nRF52840), SSD1306 OLEDs, one WS2812 strip
per half (6 LEDs at the back + 21 under the keys).

## Layout of this repo

- `config/korz.keymap`, `config/korz.conf`, `config/mouse.dtsi`, `config/west.yml` - keymap and settings
- `config/keyfx_config.h` - **all light configuration** (geometry, indicator rules, effects, triggers)
- `boards/shields/korz/` - the keyboard itself: matrix, OLED, WS2812 strip, pins
- `modules/keyfx/` - the light engine. Generic: no keyboard name, no layout. Everything it
  needs to know about your board comes from `config/keyfx_config.h` (section 2) and from the
  shield's devicetree (`keyfx,led-overlay` node + a `keyfx` behavior node, both in the shield).
- `zephyr/module.yml` - makes this repo a ZMK module and pulls in `modules/keyfx`
- `legacy/` - the old board definition and OLED widgets (not built)

## Using keyfx in another project / its own repo

1. Copy `modules/keyfx` to its own repository (the `zephyr/module.yml` inside it is ready).
2. Add it to the other project's `west.yml` as a project.
3. In that keyboard's shield/overlay add the two devicetree nodes (see this shield's
   `korz.dtsi` and `boards/nice_nano_nrf52840_zmk.overlay`), and copy `config/keyfx_config.h`
   (edit section 2 for the new layout).
4. In this repo, replace the three keyfx lines in `zephyr/module.yml` with only
   `settings: board_root: .`.

`dt-bindings/zmk/korz_fx.h` inside the module is a one-line alias kept so the keymap's
original `#include` keeps working; new projects include `<dt-bindings/zmk/keyfx.h>`.

## Keymap commands (`#include <dt-bindings/zmk/korz_fx.h>`)

- `&kfx KFX_SPLASH_TOG 0` toggle keystroke splash, `KFX_SPLASH_NEXT`, `KFX_SPLASH_ON/OFF`
- `&kfx KFX_IND_TOG 0` indicator lights on/off
- `&kfx KFX_PLAY n` play effect number n (order in CI_EFFECTS, from 0)

Effects that follow ZMK's own combos, macros and behaviors are listed in `CI_ACTIONS`
(`config/keyfx_config.h`); the keymap needs no changes for that.
