/* ===========================================================================
 * keyfx_config.h - the ONE file you edit to choose keys, colours, effects and leader lights.
 * Your keymap (the .keymap file in config/) stays untouched.
 *
 * CONTENTS
 *   1. General settings      brightness, speeds, colour names
 *   2. Keyboard geometry     where every key and LED is
 *   3. Indicator rules       keys that light for caps lock, layers, modifiers, Bluetooth, ...
 *   4. Effects               named animations (ripple, rain, beam, snake, lightning, ...)
 *   5. Effect mixer          combine effects into one (optional)
 *   6. Keystroke splash      light effect on every key press (optional)
 *   7. Actions               play an effect when ZMK runs a combo / macro / behavior
 *   8. Leader key lights     light the keys of a leader sequence
 *   9. Triggers              effects tied to raw key presses (optional)
 *
 * HOW THE LIGHTS ARE COMBINED (bottom to top)
 *   your underglow  ->  indicator rules (3)  ->  leader lights (8)  ->  effects (4-7, added
 *   on top, they never replace anything)
 *
 * RULES FOR EDITING (they save you from build errors)
 *   - In a multi-line #define, every line ends with a backslash \  and NOTHING after it
 *     (no space, no // comment). The last line of a #define has no backslash.
 *   - Comments inside a #define must be written like this one: / * ... * /  followed by \
 *   - Key positions are ZMK positions (the numbers in your keymap and combos).
 *   - Layers are numbered as in the keymap: 0 DEFAULT, 1 NUMB, 2 MOUSE (RAISE), 3 FUNC (MOC).
 * =========================================================================== */


/* ===========================================================================
 * 1. GENERAL SETTINGS
 * =========================================================================== */

#define CI_BRIGHTNESS_PERCENT     60   /* indicator lights (section 3), 1-100            */
#define CI_FX_BRIGHTNESS_PERCENT  55   /* all effects (sections 4-7), 1-100              */
#define CI_BREATHE_MS             2400 /* one CI_ANIM_BREATHE cycle                      */
#define CI_BLINK_MS               300  /* one CI_ANIM_BLINK cycle                        */
#define CI_SPLASH_DEFAULT_ON      0    /* keystroke splash (section 6) on after boot?    */

/* Colours as 0xRRGGBB. Use these names anywhere a colour is wanted; add your own freely.
 * CI_RAINBOW is also valid for effects and rules: a moving rainbow. */
#define CI_RED     0xFF0000
#define CI_ORANGE  0xFF6000
#define CI_YELLOW  0xFFC000
#define CI_GREEN   0x00FF00
#define CI_CYAN    0x00FFFF
#define CI_BLUE    0x0000FF
#define CI_PURPLE  0x8000FF
#define CI_PINK    0xFF30A0
#define CI_WHITE   0xFFFFFF


/* ===========================================================================
 * 2. KEYBOARD GEOMETRY (the only place that knows your layout)
 *
 *   CI_KEY(half, led, x, y)    one entry per key POSITION, in position order
 *                              (first entry = position 0, second = position 1, ...)
 *       half  CI_LEFT = the central half (the one running the keymap), CI_RIGHT = the other
 *       led   index of that key's LED along its half's strip (0 = first after the data pin)
 *       x, y  place on the board in tenths of a key pitch (10 = one key to the right / down).
 *             Effects use these numbers: distances, directions and speeds are all measured in
 *             them, so a distance of 50 means "five keys".
 *
 *   CI_EXTRA(half, led, x, y)  an LED that is not under a key (your 6 back LEDs).
 *
 * Positions of this board (Corne clone, 42 keys):
 *    0  1  2  3  4  5        6  7  8  9 10 11
 *   12 13 14 15 16 17       18 19 20 21 22 23
 *   24 25 26 27 28 29       30 31 32 33 34 35
 *            36 37 38       39 40 41
 * =========================================================================== */

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


/* ===========================================================================
 * 3. INDICATOR RULES
 *
 * A rule lights its keys while a condition is true. Later rules paint over earlier ones.
 *
 *   CI_RULE(source, argument, colour, animation, layer, key positions...)
 *
 *   source            argument     lit while...
 *   CI_CAPS_ON        0            Caps Lock is on
 *   CI_NUM_ON         0            Num Lock is on
 *   CI_NUM_OFF        0            Num Lock is OFF
 *   CI_CAPS_WORD      0            caps word is active
 *   CI_LAYER          layer n      layer n is active (num_word and sticky layers count)
 *   CI_MOD            modifiers    a modifier is down, physical or sticky: CI_MOD_SHIFT /
 *                                  CTRL / ALT / GUI, or one side only: CI_MOD_LSHIFT,
 *                                  CI_MOD_RGUI, ... (OR them together with |)
 *   CI_BT_CONNECTED  profile 0-4   that Bluetooth profile is selected and connected
 *   CI_BT_SEARCHING  profile 0-4   selected but not connected (advertising)
 *   CI_SPLASH_ON      0            keystroke splash mode (section 6) is on
 *
 *   animation   CI_ANIM_SOLID, CI_ANIM_BREATHE (slow swell), CI_ANIM_BLINK
 *   layer       CI_ALWAYS   show on every layer
 *               n           show whenever layer n is active (the base layer is always
 *                           active, so 0 means "always")
 *               CI_ONLY(n)  show only while n is the TOP active layer. Use it to put the same
 *                           indicator on different keys on different layers:
 *                             CI_RULE(CI_MOD, CI_MOD_CTRL, CI_CYAN, CI_ANIM_BLINK, CI_ONLY(0), 24, 19)
 *                             CI_RULE(CI_MOD, CI_MOD_CTRL, CI_CYAN, CI_ANIM_BLINK, CI_ONLY(3),  7,  8)
 *
 * HELD KEYS    CI_HELD_RULE(held key, after how many ms, colour, animation, layer, lit keys...)
 *              lights keys while another key is being held.
 *
 * PER-RULE OPTIONS   CI_RULEX(source, arg, colour, animation, layer, CI_OPTS(...), keys...)
 *   .brightness  percent, for this rule only         .period_ms    breathe / blink period
 *   .fade_in_ms  time to come up                     .fade_out_ms  time to fade away
 *   .color2, .color3   more colours: the keys, in the order you list them, blend from the
 *                      colour to colour2 (and colour3). CI_RAINBOW gives a moving rainbow.
 *   Example:
 *     CI_RULEX(CI_CAPS_ON, 0, CI_RED, CI_ANIM_SOLID, CI_ALWAYS, \
 *              CI_OPTS(.color2 = CI_ORANGE, .fade_in_ms = 300), 36, 19)
 *
 * (Use the ANIMATION names here: CI_ANIM_*. The effect names CI_FX_* belong to section 4.)
 * =========================================================================== */

#define CI_RULES \
	/* Caps Lock key, Num Lock key (lit while Num Lock is off) */ \
	CI_RULE(CI_CAPS_ON,  0, CI_RED,    CI_ANIM_SOLID,    CI_ALWAYS, 36, 19) \
	CI_RULE(CI_NUM_OFF,  0, CI_ORANGE, CI_ANIM_SOLID,    CI_ALWAYS, 29) \
	/* Layer indicators */ \
	CI_RULE(CI_LAYER,    1, CI_YELLOW, CI_ANIM_BREATHE,  CI_ALWAYS, 2, 3, 4, 14, 15, 16, 26, 27, 28, 7, 8, 9, 19, 20, 21, 31, 32, 33) \
	CI_RULE(CI_LAYER,    2, CI_BLUE,   CI_ANIM_BLINK,    CI_ALWAYS, 15, 26, 27, 28, 37, 38) \
	CI_RULE(CI_LAYER,    2, CI_GREEN,  CI_ANIM_SOLID,    CI_ALWAYS, 20, 31, 32, 33) \
	/* Bluetooth profile keys on the FUNC layer (positions of &bt BT_SEL 0..4): */ \
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
	/* Caps word: the letter keys breathe purple */ \
	CI_RULE(CI_CAPS_WORD, 0, CI_PURPLE, CI_ANIM_BREATHE, CI_ALWAYS, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34) \
	/* Modifiers */ \
	CI_RULE(CI_MOD, CI_MOD_CTRL,  CI_CYAN, CI_ANIM_BLINK, CI_ALWAYS, 24, 19) \
	CI_RULE(CI_MOD, CI_MOD_SHIFT, CI_PINK, CI_ANIM_BLINK, CI_ALWAYS, 23) \
	/* Splash mode indicator: key 11 (the toggle on the FUNC layer) */ \
	CI_RULE(CI_SPLASH_ON, 0, CI_PINK, CI_ANIM_SOLID, 3, 11) \
	/* Held keys: holding key 23 (no delay) lights key 11 - not key 23 itself */ \
	CI_HELD_RULE(23, 0,   CI_CYAN,   CI_ANIM_SOLID,   CI_ALWAYS, 11) \
	/* holding key 37 for more than 300 ms lights keys 3 and 37 */ \
	CI_HELD_RULE(37, 300, CI_ORANGE, CI_ANIM_BREATHE, CI_ALWAYS, 3, 37)
	/* More examples - copy above the last rule (keep the trailing backslashes):
	CI_RULEX(CI_LAYER, 1, CI_RED, CI_ANIM_BREATHE, CI_ALWAYS, \
		CI_OPTS(.color2 = CI_BLUE, .period_ms = 3000, .fade_in_ms = 400, .fade_out_ms = 600, .brightness = 40), \
		2, 3, 4, 14, 15, 16) \
	CI_RULE(CI_MOD, CI_MOD_GUI,   CI_YELLOW, CI_ANIM_BREATHE, CI_ALWAYS, 36) \
	CI_RULE(CI_CAPS_WORD, 0, CI_RED, CI_ANIM_BLINK, CI_ALWAYS, 36) \
	*/


/* ===========================================================================
 * 4. EFFECTS
 *
 * An effect is a named animation. Name it here, then use it as FX_<name> in sections 5-8, or
 * from the keymap with "&kfx KFX_PLAY n" (n = its number: the first effect below is 0).
 *
 * Colour: 0xRRGGBB, a CI_ name from section 1, or CI_RAINBOW.
 * Keys in effects are written CI_AT(position) (so that 0 can mean "not set").
 * Directions are degrees:  0 = right,  90 = down,  180 = left,  270 = up.
 * Distances / sizes are tenths of a key pitch (10 = one key, like the geometry).
 *
 * --- A) CLASSIC EFFECTS:  CI_EFFECT(NAME, type, colour, duration_ms, arg) ---------------
 *   type          what it does                                    arg
 *   CI_FX_RIPPLE  ring growing from the origin key                max radius (0 = whole board)
 *   CI_FX_WAVE    vertical wall sweeping away from the origin     -
 *   CI_FX_RAIN    drops falling on some LEDs                      % of LEDs (default 30)
 *   CI_FX_BLOOM   some LEDs swell and fade at random              % of LEDs (default 30)
 *   CI_FX_FLASH   everything flashes and fades                    -
 *   CI_FX_SPLASH  small quick ripple (for keystrokes)             radius (default 30)
 *
 * --- B) MOTION EFFECTS:  CI_EFFECTX(NAME, type, colour, duration_ms, .option = value, ...)
 *   Every option is optional; leave out what you do not need.
 *
 *   type             what it does
 *   CI_FX_BEAM       laser: a bright head with a trail shot from a key. Several shots can fan
 *                    out (.count / .spread). .invert makes the shots travel back to the start
 *                    (converging).
 *   CI_FX_SNAKE      the snake game: exactly one key wide, it walks the key grid one key at a
 *                    time (right, left, up, down - never diagonally), turning at random. With
 *                    .to it wanders towards the end key and stops there.
 *   CI_FX_SPIRAL     a trail spiralling outwards from the start key; .invert spirals inwards.
 *   CI_FX_BALL       a ball dropped from the start key that bounces away along the board,
 *                    parabola by parabola, losing height each bounce.
 *   CI_FX_LIGHTNING  a jagged bolt that strikes quickly, then flickers and fades, with
 *                    random brightness (and random direction with .random).
 *   CI_FX_SWIPE      a wall of light sweeping in any direction. .bidir spreads both ways
 *                    from the start line; .invert contracts instead of expanding; .fill keeps
 *                    everything the wall has passed lit until the end.
 *   CI_FX_MATRIX     columns of falling light with a shimmering shine, as in the film.
 *
 *   option      meaning                                                       default
 *   .from       CI_AT(key) where it starts                                    the key that
 *                                                                             triggered it
 *   .to         CI_AT(key) where it ends: sets direction AND distance         -
 *   .angle      direction in degrees (ignored when .to or .random is set)     0 (lightning
 *                                                                             90, matrix 90)
 *   .dist       distance or radius                                            automatic
 *   .count      beam: number of shots   lightning: number of bolts            1
 *               snake: number of steps it walks                                14
 *               matrix: % of columns that rain                                60
 *   .spread     beam: degrees covered by the fan of shots, centred on .angle  0
 *   .size       trail length: % of the path (beam, spiral, ball); number of KEYS
 *               for the snake                                                 25 / snake 4
 *   .width      thickness of the beam / bolt / wall                           beam 14,
 *                                                                             bolt 7, wall 18
 *   .wiggle     lightning: sideways amount                                    20
 *               snake: % chance of turning or wandering at each step          30
 *   .turns      spiral: number of turns                                       2
 *   .height     ball: height it is dropped from                               60
 *   .bounce     ball: bounciness, % of the height kept after each bounce      60
 *   .accel      -100..100: + starts slow and ends fast, - starts fast and     0
 *               ends slow (this is the acceleration / deceleration)
 *   .invert     1 = run the other way (converge / contract / inwards)         0
 *   .random     1 = a new random direction every time                         0
 *   .bidir      swipe only: 1 = spread both ways from a vertical line. The line is the  0
 *               middle between .from and .to (e.g. keys 38 and 39 = the middle of the
 *               board), or the middle of the board when .from is not set. With .invert
 *               the light closes in from both outer edges to that line.
 *   .fill       swipe only: 1 = leave what the wall passed lit                0
 *
 *   For ALL motion effects:
 *   .margin     keep this many keys away from the edges: lights outside the inner      0
 *               area stay dark and the snake does not walk there. Rows and columns
 *               are counted in keys (1 = skip the outer ring of keys and the back
 *               LEDs). Example: .margin = 1 on this board leaves 10 columns x 2 rows.
 *   .loops      repeat the movement this many times within the duration                1
 *   .pingpong   1 = every second loop runs backwards (a spiral that goes out, then in) 0
 *   .breathe_ms swell and dim with this period (ms)                                    off
 *   .nofade     1 = do not fade out at the end (the last frame stays)                  0
 *   .head       colour of the leading light; .color2 the middle of the trail. The      none
 *               trail runs colour -> .color2 -> .head, e.g. orange, yellow, white.
 *               (beam, snake, spiral, ball and matrix)
 *   .center     1 = start in the middle of the keys instead of at a key                0
 *   .aspect     spiral: width in % of its height. Boards are wider than tall, so       100
 *               150-250 makes the spiral an ellipse that fills the keyboard.
 *   Tip: to light exactly one key at a time use .width = 6 (the lit area is then
 *   smaller than the 10 units between keys); the snake is always one key.
 *
 *   Matrix only (CI_FX_MATRIX):
 *   (.head, above, is the white wisp: .head = CI_WHITE)
 *   .count      fewest columns that rain, % (random between .count and .count_max)   60
 *   .count_max  most columns that rain, % (chosen again at every start)             = .count
 *   .speed_min, .speed_max   every column falls at its own random speed in this     70, 130
 *               range, percent of normal (100 = normal)
 *   .dist       length of the trail, tenths of a key                                 45
 *   .flicker    flicker strength, % (0 = steady). It grows toward the tail of        40
 *               each column and every column flickers at its own rate
 *
 *   Examples (all in the list below):
 *     a laser fired from key 5 to the lower left, 3 shots fanning over 90 degrees
 *     CI_EFFECTX(LASER, CI_FX_BEAM, CI_RED, 900, .from = CI_AT(5), .angle = 225, .count = 3, .spread = 90, .accel = 40)
 *     a snake from key 12 to key 35
 *     CI_EFFECTX(SNAKE, CI_FX_SNAKE, CI_GREEN, 1500, .from = CI_AT(12), .to = CI_AT(35))
 *
 * THINGS TO CONSIDER WHEN CHOOSING SIZES (this board: 12 columns x 4 rows, keys 10 apart,
 * so the keys span x = 0..110 and y = 0..30; the 12 back LEDs sit one row above, y = -10)
 *   - Width / size: .width = 6 lights about one key; 14 (the beam default) about two to three
 *     keys; 30 or more lights a whole block, and a width as big as the board (50+) makes the
 *     effect look like a flash - you will not see it move. Likewise a trail (.size, .dist)
 *     longer than the board never shows its head or tail.
 *   - Speed: the effect crosses its distance in its duration. 10 keys in 600 ms is a blur;
 *     in 1500-2500 ms you can follow it. A snake takes duration / steps per key.
 *   - Height: the board is only 4 rows tall. Vertical beams, bolts and spirals have just
 *     30 units to travel; use .aspect for spirals and .angle near 0 or 180 for beams.
 *   - Distances are tenths of a key: .dist = 50 is five keys, .height = 30 is three rows.
 *   - .margin = 0 (the default) means NO margin: the effect may use every key and the back
 *     LEDs. .margin = 1 keeps one key away from every edge (and drops the back LEDs).
 *   - A start key at an edge (e.g. position 0) with an angle pointing outwards shows nothing.
 *
 * Effects of both kinds go in the same list, and every line but the last ends with a backslash.
 * =========================================================================== */

#define CI_EFFECTS \
	/* ---- classic effects ---- */ \
	CI_EFFECT(LOGOUT,     CI_FX_RIPPLE, CI_RAINBOW, 3500, 90) \
	CI_EFFECT(UNLOCK,     CI_FX_RAIN,   CI_CYAN,    1600, 60) \
	CI_EFFECT(KEY_SPLASH, CI_FX_SPLASH, CI_WHITE,    450, 0)  \
	CI_EFFECT(KEY_RIPPLE, CI_FX_RIPPLE, CI_PINK,     900, 70) \
	CI_EFFECT(KEY_BLOOM,  CI_FX_BLOOM,  CI_RAINBOW, 1700, 90) \
	CI_EFFECT(RDEJ,       CI_FX_RAIN,   CI_ORANGE,  1600, 80) \
	CI_EFFECT(KWAIT,      CI_FX_BLOOM,  CI_BLUE,    6700, 90) \
	/* ---- motion effects: samples, nothing uses them until you add them in sections 5-8 ---- */ \
	CI_EFFECTX(LASER,   CI_FX_BEAM,      CI_RED,    900,  .angle = 90, .count = 3, .spread = 120, .accel = 40) \
	CI_EFFECTX(SNAKE,   CI_FX_SNAKE,     CI_GREEN,  2500, .from = CI_AT(0), .to = CI_AT(35), .size = 4, .count = 16, .wiggle = 30, .head = CI_WHITE) \
	CI_EFFECTX(SPIRAL,  CI_FX_SPIRAL,    CI_CYAN,   1500, .turns = 2) \
	/* shown just before &bootloader reboots a half (see CI_BOOTLOADER_FX below): a spiral from the middle that loops in and out, breathing, orange - yellow - white */ \
	CI_EFFECTX(BOOT,    CI_FX_SPIRAL,    CI_ORANGE, 2400, .center = 1, .dist = 40, .aspect = 150, .turns = 3, .size = 70, .width = 13, \
		.color2 = CI_YELLOW, .head = CI_WHITE, .loops = 4, .pingpong = 1, .breathe_ms = 600, .nofade = 1) \
	CI_EFFECTX(BALL,    CI_FX_BALL,      CI_YELLOW, 2200, .height = 50, .bounce = 60, .dist = 120) \
	CI_EFFECTX(BOLT,    CI_FX_LIGHTNING, CI_WHITE,  700,  .angle = 90, .dist = 40, .wiggle = 10) \
	CI_EFFECTX(SWIPE,   CI_FX_SWIPE,     CI_BLUE,   1200, .angle = 0, .fill = 1) \
	CI_EFFECTX(SQUEEZE, CI_FX_SWIPE,     CI_PURPLE, 1200, .from = CI_AT(38), .to = CI_AT(39), .bidir = 1, .invert = 1) \
	CI_EFFECTX(MATRIX,  CI_FX_MATRIX,    CI_GREEN,  3000, .angle = 90, .head = CI_WHITE, .count = 40, .count_max = 80, \
		.speed_min = 60, .speed_max = 140, .dist = 50, .flicker = 55)


/* ===========================================================================
 * 5. EFFECT MIXER (optional)
 *
 * A mix plays several effects of section 4 together or one after another. A mix gets a name
 * and is used like any effect (FX_<name> in sections 6-8).
 *
 *   #define CI_MIXES \
 *       CI_MIX(NAME, \
 *           CI_PART(EFFECT, options...) \
 *           CI_PART(EFFECT, options...))
 *
 * Parts start together unless .start_ms delays them (a pause, or a sequence).
 * Options of a part (all optional, written .name = value):
 *   .start_ms      wait this long before the part starts
 *   .duration_ms   play the effect over this long instead of its own duration
 *   .speed         percent, 100 = normal              .brightness  percent of the effect's
 *   .accel         change of speed per second in percent: + speeds up, - slows down
 *   .fade_in_ms    .fade_out_ms    fade the part in and out
 *   .pulse_ms      flash on and off with this period  .breathe_ms  swell and dim
 *   .repeat        number of times                    .pause_ms    pause between repeats
 *   .origin        CI_AT(position): start from this key instead of the trigger key
 *   .color         use this colour; add .color2 for a left-to-right gradient
 * The mix lasts until its last part ends. A part must be a plain effect of section 4 (the
 * motion effects work too), not another mix.
 *
 * Example (uncomment to use it as FX_CELEBRATE):
 *
 * #define CI_MIXES \
 *     CI_MIX(CELEBRATE, \
 *         CI_PART(KEY_RIPPLE, .fade_out_ms = 400) \
 *         CI_PART(LASER, .start_ms = 300, .color = CI_PINK) \
 *         CI_PART(KEY_BLOOM, .start_ms = 900, .pulse_ms = 200, .repeat = 2, .pause_ms = 200))
 * =========================================================================== */


/* ===========================================================================
 * 6. KEYSTROKE SPLASH
 *
 * An effect on every key press. Toggle it with &kfx KFX_SPLASH_TOG and change the style with
 * &kfx KFX_SPLASH_NEXT. List the styles to cycle through, with the FX_ prefix. Keep them short
 * (under half a second) so fast typing stays readable.
 * =========================================================================== */

#define CI_SPLASH_STYLES FX_KEY_SPLASH, FX_KEY_RIPPLE, FX_KEY_BLOOM


/* ===========================================================================
 * 7. ACTIONS: effects that follow what ZMK actually runs (the keymap stays untouched)
 *
 *   CI_ON_COMBO_NAME(EFFECT, combo)         the combo named `combo` in the keymap fires
 *   CI_ON_BEHAVIOR(EFFECT, label)           ZMK runs the behavior with that label
 *                                           (macro, tap dance, mod-morph, caps word, ...)
 *   CI_ON_BINDING(EFFECT, label, p1, p2)    ... with exactly these parameters, e.g.
 *                                           CI_ON_BINDING(X, kp, LC(LA(DEL)), 0)
 *                                           Always give both parameters; 0 when unused.
 *
 * EFFECT is the NAME of an effect of section 4 (or a mix of section 5). Names from the keymap
 * are written as the devicetree sees them: combos in lowercase (the combo LOGOFF is `logoff`),
 * behaviors by their label (td9, SHUT, caps_word...).
 *
 * The effect starts at the combo's first key (or, for behaviors, the key that was pressed)
 * and plays exactly when ZMK runs the action; behaviors that run inside others (a tap dance
 * running a macro) count too. Actions run in parallel with the indicator rules of section 3.
 * =========================================================================== */

#define CI_ACTIONS \
	CI_ON_COMBO_NAME(LOGOUT, logoff) \
	CI_ON_COMBO_NAME(KEY_BLOOM, lreboot) \
	CI_ON_COMBO_NAME(KEY_SPLASH, rbksp) \
	CI_ON_BINDING(UNLOCK, kp, LC(LA(DEL)), 0) \
	CI_ON_COMBO_NAME(RDEJ, lbksp) \
	CI_ON_COMBO_NAME(RDEJ, rdelet) \
	CI_ON_BEHAVIOR(KWAIT, pnt2)
	/* More examples:
	CI_ON_BEHAVIOR(LASER, caps_word) \
	CI_ON_BEHAVIOR(UNLOCK, SHUT) \
	*/


/* ---------------------------------------------------------------------------
 * 7b. BOOTLOADER LIGHT
 *
 * When you press a key bound to &bootloader, the reboot is held back until this effect has
 * played, so you SEE that the half is about to enter bootloader mode. Both halves play it,
 * then the half you chose reboots. Works for &bootloader on a key, a hold-tap or a tap dance;
 * it does not work for a combo that reboots (ZMK runs those on the left half).
 *
 * Note: once a half is in bootloader mode the firmware (and so this animation) is no longer
 * running, so the animation cannot keep going inside the bootloader. The effect's last frame
 * is kept (.nofade = 1) and may stay lit on the LEDs, depending on your board's LED power.
 *
 *   CI_BOOTLOADER_FX        the effect to play (FX_NONE = off, nothing is delayed)
 *   CI_BOOTLOADER_DELAY_MS  how long to wait before rebooting (0 = the effect's length)
 * ------------------------------------------------------------------------- */

#define CI_BOOTLOADER_FX FX_BOOT
/* #define CI_BOOTLOADER_DELAY_MS 2400 */


/* ===========================================================================
 * 8. LEADER KEY LIGHTS (needs the urob zmk-leader-key module)
 *
 * When a leader behavior starts, every key used by its sequences lights up. Type a key and
 * only the sequences that still match stay lit; the key you typed goes dark. Anything that
 * matches nothing, or waiting too long, switches the lights off; finishing a sequence plays
 * the "done" effect. The sequences are read from your keymap: nothing is typed twice.
 *
 *   CI_LEADER(label of the leader behavior, default colour, effect when done, effect when
 *             aborted)           FX_NONE = no effect. FX_NONE can be replaced by any FX_name.
 *   CI_LEADER_SEQ(sequence node name, colour, effect when this one is done)
 *                                optional: its own colour/effect for one sequence
 *                                (FX_NONE = the leader's "done" effect)
 *
 * Leave both lists empty to turn the feature off. Up to 15 colours.
 * =========================================================================== */

#define CI_LEADERS \
	CI_LEADER(leader, CI_WHITE, FX_NONE, FX_NONE)

#define CI_LEADER_SEQS \
	CI_LEADER_SEQ(tick, CI_GREEN,  FX_NONE) \
	CI_LEADER_SEQ(usb,  CI_BLUE,   FX_NONE) \
	CI_LEADER_SEQ(ble,  CI_CYAN,   FX_NONE) \
	CI_LEADER_SEQ(btc,  CI_RED,    FX_NONE)

/* Where the sequence keys are on the keyboard, per layer. Only these keys can light, and only
 * these (plus the layer keys in CI_LEADER_IGNORE_POS) can be typed during a sequence without
 * aborting it. A key is looked up in the highest active layer that lists its position, so
 * digits light only while the layer that holds them is active.
 *   CI_LKEYS(layer number, CI_LKEY(KEY, position), ...) ...
 * Key names are ZMK's (A..Z, N0..N9, KP_N0.., LSHFT, ...). Add the modifiers you use inside a
 * sequence (for example LSHFT) so typing them does not abort it. Each CI_LKEYS line needs a
 * trailing backslash, except the last. */
#define CI_LEADER_KEYS \
	CI_LKEYS(0, /* base layer: letters */ \
		CI_LKEY(Q, 1),  CI_LKEY(W, 2),  CI_LKEY(E, 3),  CI_LKEY(R, 4),  CI_LKEY(T, 5), \
		CI_LKEY(A, 13), CI_LKEY(S, 14), CI_LKEY(D, 15), CI_LKEY(F, 16), CI_LKEY(G, 17)) \
	CI_LKEYS(3, /* NUMB layer: digits */ \
		CI_LKEY(N1, 1), CI_LKEY(N2, 2), CI_LKEY(N3, 3), CI_LKEY(N4, 4), CI_LKEY(N5, 5))

/* Leader options (remove the comment marks to use them) */
/* #define CI_LEADER_IGNORE_POS 41, 40, */      /* positions of your layer keys: pressing one does not abort the
                                                   sequence and the lit keys follow the new layer. Every number
                                                   needs a comma after it, also the last. */
/* #define CI_LEADER_STRICT 1 */               /* 1: a key that is not in CI_LEADER_KEYS aborts the sequence
                                                   (default 0: such keys are ignored) */
/* #define CI_LEADER_TIMEOUT_MS 25000 */       /* lights go out after this long (default 15000, 0 = never) */
/* #define CI_LEADER_DIM_OTHERS 0 */           /* 1 (default): every other light goes off during a sequence */
/* #define CI_LEADER_ANIM CI_ANIM_BREATHE */   /* solid (default), breathe or blink */
/* #define CI_LEADER_BRIGHTNESS_PERCENT 80 */
/* #define CI_LEADER_DEBUG_FX FX_KEY_RIPPLE */ /* test aid: plays this effect whenever a leader start is seen */


/* ===========================================================================
 * 9. TRIGGERS BY KEY POSITION (optional; they only watch key presses)
 *
 *   CI_ON_COMBO(NAME, window_ms, positions...)    keys pressed together
 *   CI_ON_TAPS(NAME, count, window_ms, position)  key tapped <count> times in a row
 *   CI_ON_PRESS(NAME, position)                   key pressed
 *
 * They do not know whether ZMK acted on the presses, so prefer section 7.
 * =========================================================================== */

#define CI_TRIGGERS
