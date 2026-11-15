/*
 * keyfx light rules - EDIT THIS FILE to choose keys, colours and effects.
 *
 * Key positions are the ZMK positions used in the keymap and combos (see section 2).
 *
 * Each half paints only the keys that physically belong to it. Layers are numbered
 * as in the keymap: 0 DEFAULT, 1 NUMB, 2 MOUSE (RAISE), 3 FUNC (MOC).
 */

/* ---------------------------------------------------------------------------
 * 1. General settings
 * ------------------------------------------------------------------------- */

#define CI_BRIGHTNESS_PERCENT     60   /* indicator lights, 1-100 */
#define CI_FX_BRIGHTNESS_PERCENT  55   /* ripples, rain, splashes, 1-100 */
#define CI_BREATHE_MS             2400 /* one breathing cycle */
#define CI_BLINK_MS               300  /* one blink cycle */
#define CI_SPLASH_DEFAULT_ON      0    /* keystroke splash on after boot? */

/* Colours as 0xRRGGBB (CI_RAINBOW is also valid for effects) */
#define CI_RED     0xFF0000
#define CI_ORANGE  0xFF6000
#define CI_YELLOW  0xFFC000
#define CI_GREEN   0x00FF00
#define CI_CYAN    0x00FFFF
#define CI_BLUE    0x0000FF
#define CI_PURPLE  0x8000FF
#define CI_PINK    0xFF30A0
#define CI_WHITE   0xFFFFFF

/* ---------------------------------------------------------------------------
 * 2. Keyboard geometry (the only place that knows your layout)
 *
 *   CI_KEY(half, led, x, y)    one entry per key POSITION, listed in position order
 *                              (entry 0 = position 0, entry 1 = position 1, ...)
 *       half  CI_LEFT = the central half (the one running the keymap), CI_RIGHT = other
 *       led   index of that key's LED along its half's strip (0 = first after data pin)
 *       x, y  place on the board in tenths of a key pitch (10 = one key to the right /
 *             down); effects use these to travel across the keyboard
 *
 *   CI_EXTRA(half, led, x, y)  an LED that is not under a key (backlight strip, etc.)
 *
 * Positions here (Corne clone, 42 keys):
 *   0  1  2  3  4  5        6  7  8  9 10 11
 *  12 13 14 15 16 17       18 19 20 21 22 23
 *  24 25 26 27 28 29       30 31 32 33 34 35
 *           36 37 38       39 40 41
 * ------------------------------------------------------------------------- */

#define CI_KEYS \
	CI_KEY(CI_LEFT ,24,  0, 0) CI_KEY(CI_LEFT ,23, 10, 0) CI_KEY(CI_LEFT ,18, 20, 0) CI_KEY(CI_LEFT ,17, 30, 0) CI_KEY(CI_LEFT ,10, 40, 0) CI_KEY(CI_LEFT , 9, 50, 0) \
	CI_KEY(CI_RIGHT, 9, 60, 0) CI_KEY(CI_RIGHT,10, 70, 0) CI_KEY(CI_RIGHT,17, 80, 0) CI_KEY(CI_RIGHT,18, 90, 0) CI_KEY(CI_RIGHT,23,100, 0) CI_KEY(CI_RIGHT,24,110, 0) \
	CI_KEY(CI_LEFT ,25,  0,10) CI_KEY(CI_LEFT ,22, 10,10) CI_KEY(CI_LEFT ,19, 20,10) CI_KEY(CI_LEFT ,16, 30,10) CI_KEY(CI_LEFT ,11, 40,10) CI_KEY(CI_LEFT , 8, 50,10) \
	CI_KEY(CI_RIGHT, 8, 60,10) CI_KEY(CI_RIGHT,11, 70,10) CI_KEY(CI_RIGHT,16, 80,10) CI_KEY(CI_RIGHT,19, 90,10) CI_KEY(CI_RIGHT,22,100,10) CI_KEY(CI_RIGHT,25,110,10) \
	CI_KEY(CI_LEFT ,26,  0,20) CI_KEY(CI_LEFT ,21, 10,20) CI_KEY(CI_LEFT ,20, 20,20) CI_KEY(CI_LEFT ,15, 30,20) CI_KEY(CI_LEFT ,12, 40,20) CI_KEY(CI_LEFT , 7, 50,20) \
	CI_KEY(CI_RIGHT, 7, 60,20) CI_KEY(CI_RIGHT,12, 70,20) CI_KEY(CI_RIGHT,15, 80,20) CI_KEY(CI_RIGHT,20, 90,20) CI_KEY(CI_RIGHT,21,100,20) CI_KEY(CI_RIGHT,26,110,20) \
	CI_KEY(CI_LEFT ,14, 30,30) CI_KEY(CI_LEFT ,13, 40,30) CI_KEY(CI_LEFT , 6, 50,30) CI_KEY(CI_RIGHT, 6, 60,30) CI_KEY(CI_RIGHT,13, 70,30) CI_KEY(CI_RIGHT,14, 80,30) \
	/* end */

#define CI_EXTRA_LEDS \
	CI_EXTRA(CI_LEFT,  0,  50, -10) CI_EXTRA(CI_LEFT,  1,  40, -10) CI_EXTRA(CI_LEFT,  2,  30, -10) \
	CI_EXTRA(CI_LEFT,  3,  20, -10) CI_EXTRA(CI_LEFT,  4,  10, -10) CI_EXTRA(CI_LEFT,  5,   0, -10) \
	CI_EXTRA(CI_RIGHT, 0,  60, -10) CI_EXTRA(CI_RIGHT, 1,  70, -10) CI_EXTRA(CI_RIGHT, 2,  80, -10) \
	CI_EXTRA(CI_RIGHT, 3,  90, -10) CI_EXTRA(CI_RIGHT, 4, 100, -10) CI_EXTRA(CI_RIGHT, 5, 110, -10)

/* ---------------------------------------------------------------------------
 * 3. Indicator rules (lit while their condition is true)
 *
 *   CI_RULE(source, argument, colour, animation, when, key positions...)
 *
 *   source        argument
 *   CI_CAPS_ON    0              Caps Lock is on
 *   CI_NUM_OFF    0              Num Lock is OFF
 *   CI_NUM_ON     0              Num Lock is on
 *   CI_LAYER      layer number   that layer is active (also covers num_word and
 *                                sticky layers, which are layers)
 *   CI_MOD        modifier mask  a modifier is active, physical or sticky:
 *                                CI_MOD_SHIFT/CTRL/ALT/GUI or CI_MOD_LSHIFT,
 *                                CI_MOD_RGUI, ... (OR them together)
 *   CI_CAPS_WORD  0              caps word is active
 *   CI_BT_CONNECTED  profile 0-4 that BT_SEL profile is selected and connected
 *   CI_BT_SEARCHING  profile 0-4 selected but not connected (advertising)
 *   CI_SPLASH_ON  0              keystroke splash mode is on
 *
 *   animation     CI_ANIM_SOLID, CI_ANIM_BREATHE, CI_ANIM_BLINK
 *   when          CI_ALWAYS, or a layer number: only show while that layer is active
 *
 * Later rules paint over earlier ones.
 * ------------------------------------------------------------------------- */

#define CI_RULES \
	/* Caps lock key, Num Lock key (lit while Num Lock is off) */ \
	CI_RULE(CI_CAPS_ON,  0, CI_RED,    CI_ANIM_SOLID,    CI_ALWAYS, 36, 19) \
	CI_RULE(CI_NUM_OFF,  0, CI_ORANGE, CI_ANIM_SOLID,    CI_ALWAYS, 29) \
	CI_RULE(CI_LAYER, 	 1, CI_YELLOW, CI_ANIM_BREATHE,  CI_ALWAYS, 2, 3, 4, 14, 15, 16, 26, 27, 28, 7, 8, 9, 19, 20, 21, 31, 32, 33) \
    CI_RULE(CI_LAYER,	 2, CI_BLUE,   CI_FX_FLASH,		 CI_ALWAYS,	15, 26, 27, 28, 37, 38) \
	CI_RULE(CI_LAYER,	 2, CI_GREEN,   CI_ANIM_SOLID,		 CI_ALWAYS,	20, 31, 32, 33) \
	/* Bluetooth profile keys on the FUNC layer (positions of &bt BT_SEL 0..4):  */ \
	/* selected + connected = breathing blue, selected but not connected = breathing orange */ \
	CI_RULE(CI_BT_CONNECTED, 0, CI_BLUE,   CI_ANIM_BREATHE, 3, 1) \
	CI_RULE(CI_BT_CONNECTED, 1, CI_BLUE,   CI_ANIM_BREATHE, 3, 2) \
	CI_RULE(CI_BT_CONNECTED, 2, CI_BLUE,   CI_ANIM_BREATHE, 3, 13) \
	CI_RULE(CI_BT_CONNECTED, 3, CI_BLUE,   CI_ANIM_BREATHE, 3, 14) \
	CI_RULE(CI_BT_CONNECTED, 4, CI_BLUE,   CI_ANIM_BREATHE, 3, 25) \
	CI_RULE(CI_BT_SEARCHING, 0, CI_ORANGE, CI_ANIM_BREATHE, 3, 1) \
	CI_RULE(CI_BT_SEARCHING, 1, CI_ORANGE, CI_ANIM_BREATHE, 3, 2) \
	CI_RULE(CI_BT_SEARCHING, 2, CI_ORANGE, CI_ANIM_BREATHE, 3, 13) \
	CI_RULE(CI_BT_SEARCHING, 3, CI_ORANGE, CI_ANIM_BREATHE, 3, 14) \
	CI_RULE(CI_BT_SEARCHING, 4, CI_ORANGE, CI_ANIM_BREATHE, 3, 25) \
	CI_RULE(CI_CAPS_WORD, 	 0, CI_PURPLE, CI_ANIM_BREATHE, CI_ALWAYS, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34) \
	CI_RULE(CI_MOD, 	CI_MOD_CTRL, CI_CYAN,   CI_FX_FLASH,   CI_ALWAYS, 24, 19) \
	CI_RULE(CI_MOD, 	CI_MOD_SHIFT, CI_PINK,   CI_FX_FLASH,   CI_ALWAYS, 23) \
	/* Splash mode indicator: key 11 (the toggle on the FUNC layer) */ \
	CI_RULE(CI_SPLASH_ON, 0, CI_PINK, CI_ANIM_SOLID, 3, 11)
	/* More examples - uncomment (keep the trailing backslashes) and adjust:
	CI_RULE(CI_LAYER, 1, CI_BLUE,  CI_ANIM_SOLID,   CI_ALWAYS, 30, 31, 32, 33, 34, 35) \
	CI_RULE(CI_LAYER, 2, CI_GREEN, CI_ANIM_SOLID,   CI_ALWAYS, 37, 38, 39) \
	CI_RULE(CI_MOD, CI_MOD_GUI,   CI_YELLOW, CI_ANIM_BREATHE, CI_ALWAYS, 36) \
	CI_RULE(CI_MOD, CI_MOD_SHIFT, CI_CYAN,   CI_ANIM_SOLID,   CI_ALWAYS, 24) \
	CI_RULE(CI_CAPS_WORD, 0, CI_RED, CI_ANIM_BLINK, CI_ALWAYS, 36) \
	*/

/* ---------------------------------------------------------------------------
 * 4. Effects
 *
 *   CI_EFFECT(NAME, type, colour, duration_ms, arg)
 *
 *   type         what it does                                 arg
 *   CI_FX_RIPPLE ring expanding from the origin key           max radius (0 = whole keyboard)
 *   CI_FX_WAVE   wall sweeping away from the origin key       -
 *   CI_FX_RAIN   drops falling on some LEDs                   % of LEDs (default 30)
 *   CI_FX_BLOOM  some LEDs swell and fade at random           % of LEDs (default 30)
 *   CI_FX_FLASH  everything flashes and fades                 -
 *   CI_FX_SPLASH small quick ripple                           radius (default 30)
 *
 *   colour       0xRRGGBB, or CI_RAINBOW
 *
 * Effect number n for "&kfx KFX_PLAY n" is the order below, starting at 0.
 * ------------------------------------------------------------------------- */

#define CI_EFFECTS \
	CI_EFFECT(LOGOUT,     CI_FX_RIPPLE, CI_RAINBOW,    3500, 90)  \
	CI_EFFECT(UNLOCK,     CI_FX_RAIN,   CI_CYAN,   1600, 60) \
	CI_EFFECT(KEY_SPLASH, CI_FX_SPLASH, CI_WHITE,   450, 0)  \
	CI_EFFECT(KEY_RIPPLE, CI_FX_RIPPLE, CI_PINK,    900, 70) \
	CI_EFFECT(KEY_BLOOM,  CI_FX_BLOOM,  CI_RAINBOW, 1700, 90) \
	CI_EFFECT(RDEJ,     CI_FX_RAIN,   CI_ORANGE,   1600, 80) \
	CI_EFFECT(KWAIT,  	  CI_FX_BLOOM,  CI_BLUE, 6700, 90)
/* ---------------------------------------------------------------------------
 * 5. Keystroke splash (toggle with &kfx KFX_SPLASH_TOG, change style with
 *    &kfx KFX_SPLASH_NEXT). List the effects to cycle through, with the FX_ prefix.
 * ------------------------------------------------------------------------- */

#define CI_SPLASH_STYLES FX_KEY_SPLASH, FX_KEY_RIPPLE, FX_KEY_BLOOM

/* ---------------------------------------------------------------------------
 * 6. Effects that follow what ZMK actually runs (the keymap stays untouched)
 *
 *   CI_ON_COMBO_NAME(EFFECT, combo)          the combo named `combo` in the keymap fires
 *   CI_ON_BEHAVIOR(EFFECT, label)            ZMK runs the behavior with that label
 *                                            (macro, tap dance, mod-morph, ...)
 *   CI_ON_BINDING(EFFECT, label, p1, p2)     ... with exactly these parameters,
 *                                            e.g. CI_ON_BINDING(X, kp, LC(LA(DEL)), 0)
 *
 *   EFFECT is the NAME of a CI_EFFECT above. Names from the keymap are written as the
 *   devicetree sees them: combos in lowercase (the combo LOGOFF is `logoff`), behaviors
 *   by their label (td9, SHUT, ...).
 *
 * The effect starts at the combo's first key (or, for behaviors, the key that was
 * pressed) and plays exactly when ZMK runs the action. Behaviors run from inside
 * others (a tap dance running a macro) count too.
 * ------------------------------------------------------------------------- */

#define CI_ACTIONS \
	CI_ON_COMBO_NAME(LOGOUT, logoff) \
	CI_ON_COMBO_NAME(KEY_BLOOM, lreboot) \
	CI_ON_COMBO_NAME(KEY_SPLASH, rbksp) \
	CI_ON_BINDING(UNLOCK, kp, LC(LA(DEL)), 0) \
	CI_ON_COMBO_NAME(RDEJ, lbksp) \
	CI_ON_COMBO_NAME(RDEJ, rdelet) \
	CI_ON_BEHAVIOR(KWAIT, pnt2)
	/* Examples:
	CI_ON_BEHAVIOR(UNLOCK, SHUT) \
	CI_ON_BINDING(UNLOCK, kp, LC(LA(DEL)), 0) \
	*/

/* ---------------------------------------------------------------------------
 * 7. Triggers by key position (optional, they only watch key presses)
 *
 *   CI_ON_COMBO(NAME, window_ms, positions...)   keys pressed together
 *   CI_ON_TAPS(NAME, count, window_ms, position) key tapped <count> times in a row
 *   CI_ON_PRESS(NAME, position)                  key pressed
 *
 * These do not know whether ZMK acted on the presses, so prefer section 6.
 * ------------------------------------------------------------------------- */

#define CI_TRIGGERS
