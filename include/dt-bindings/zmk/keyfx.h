/* Commands for the &kfx behavior:  &kfx <command> <argument> */

#define KFX_SPLASH_TOG   1  /* toggle keystroke splash effects          */
#define KFX_SPLASH_ON    2
#define KFX_SPLASH_OFF   3
#define KFX_SPLASH_NEXT  4  /* next splash style (CI_SPLASH_STYLES)     */
#define KFX_IND_TOG      5  /* indicator lights on/off                  */
#define KFX_PLAY         6  /* play effect number <argument> (0 = first CI_EFFECT) */

/*
 * Wrap an action so a light effect plays when ZMK runs it:
 *
 *   KFX_WRAP(label, "EFFECT_NAME", &behavior params)
 *   KFX_WRAP_AT(label, "EFFECT_NAME", start_position, &behavior params)
 *
 * Put the line at the top level of the keymap, then use &label wherever the original
 * binding was, e.g. in a combo:  bindings = <&label>;
 */
#define KFX_WRAP_AT(label, fx, origin, ...)                                          \
    / {                                                                              \
        behaviors {                                                                  \
            label: label {                                                           \
                compatible = "zmk,behavior-keyfx-wrap";                            \
                #binding-cells = <0>;                                                \
                fx-name = fx;                                                        \
                origin-position = <origin>;                                          \
                bindings = <__VA_ARGS__>;                                            \
            };                                                                       \
        };                                                                           \
    };

#define KFX_WRAP(label, fx, ...) KFX_WRAP_AT(label, fx, 255, __VA_ARGS__)
