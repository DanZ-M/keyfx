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

Usage
Add keyfx to the other project's `west.yml` as a project.
To load the module, add the following entries to remotes and projects in config/west.yml.

manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: DanZ-M
      url-base: https://github.com/DanZ-M
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: v0.3 # Set to desired ZMK release.
      import: app/west.yml
    - name: zmk-unicode
      remote: DanZ-M
      revision: v0.3 # Should match ZMK release.
  self:
    path: config



Note:

In that keyboard's shield add the devicetree node


    / {
    chosen {
    zmk,kscan = &kscan0;
    zmk,matrix-...................
    };
    
    behaviors {
    	/* light effects control + left/right sync, see modules/keyfx/include/dt-bindings/zmk/keyfx.h */
    	kfx: keyfx {
    		compatible = "zmk,behavior-keyfx";
    		#binding-cells = <2>;
    	};
    };


In that keyboard's overlay add the two devicetree nodes

/ {
led_overlay: led_overlay {
compatible = "keyfx,led-overlay";
strip = <&led_strip>;
chain-length = <27>;
};

    chosen {
    	zmk,underglow = &led_overlay;
    };

};

and add

#include <dt-bindings/led/led.h>

(see this shield's `your_keyboard_name.dtsi` and `boards/example_nice_nano_nrf52840_zmk.overlay`), and copy 'keyfx_config.h' into your zmk-config, config folder: `config/keyfx_config.h`
(edit section 2 for the new layout). 4. In this repo, replace the three keyfx lines in `zephyr/module.yml` with only
`settings: board_root: .`.

`dt-bindings/zmk/korz_fx.h` inside the module is a one-line alias kept so the keymap's
original `#include` keeps working; new projects include `<dt-bindings/zmk/keyfx.h>`.

## Keymap commands (`#include <dt-bindings/zmk/korz_fx.h>`)

- `&kfx KFX_SPLASH_TOG 0` toggle keystroke splash, `KFX_SPLASH_NEXT`, `KFX_SPLASH_ON/OFF`
- `&kfx KFX_IND_TOG 0` indicator lights on/off
- `&kfx KFX_PLAY n` play effect number n (order in CI_EFFECTS, from 0)

Effects that follow ZMK's own combos, macros and behaviors are listed in `CI_ACTIONS`
(`config/keyfx_config.h`); the keymap needs no changes for that.
