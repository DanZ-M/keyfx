/*
 * Action hook (central half only).
 *
 * Every combo, macro, tap dance, hold tap and key press that ZMK acts on is run through
 * zmk_behavior_invoke_binding(). The linker is told (--wrap, see CMakeLists.txt) to send
 * those calls here first. We look at what is being run, start the matching light effect
 * from CI_ACTIONS in config/keyfx_config.h, and then hand over to ZMK untouched.
 * That lets effects follow the real action without any change to the keymap.
 */

#include <stdbool.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "keyfx_internal.h"

extern int __real_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *src_binding,
                                              struct zmk_behavior_binding_event event,
                                              bool pressed);

int __wrap_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *src_binding,
                                       struct zmk_behavior_binding_event event, bool pressed) {
    if (pressed && src_binding) {
        keyfx_on_binding(src_binding, &event);
    }
    return __real_zmk_behavior_invoke_binding(src_binding, event, pressed);
}
