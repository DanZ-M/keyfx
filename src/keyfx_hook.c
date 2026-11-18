/*
 * Action hook (central half only).
 *
 * Every combo, macro, tap dance, hold tap and key press that ZMK acts on is run through
 * zmk_behavior_invoke_binding(). The linker is told (--wrap, see CMakeLists.txt) to send
 * those calls here first. We look at what is being run, start the matching light effect
 * from CI_ACTIONS in config/keyfx_config.h, and then hand over to ZMK untouched.
 * That lets effects follow the real action without any change to the keymap.
 *
 * &bootloader is the one exception: its reboot is held back until the bootloader light
 * effect (CI_BOOTLOADER_FX) has played, then ZMK runs it exactly as it would have.
 */

#include <stdbool.h>

#include <zephyr/kernel.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "keyfx_internal.h"

extern int __real_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *src_binding,
                                              struct zmk_behavior_binding_event event,
                                              bool pressed);

static struct zmk_behavior_binding boot_binding;
static struct zmk_behavior_binding_event boot_event;
static bool boot_pending;

static void boot_fn(struct k_work *work) {
    (void)__real_zmk_behavior_invoke_binding(&boot_binding, boot_event, true);
}

static K_WORK_DELAYABLE_DEFINE(boot_work, boot_fn);

int __wrap_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *src_binding,
                                       struct zmk_behavior_binding_event event, bool pressed) {
    if (src_binding && boot_pending && src_binding->behavior_dev &&
        src_binding->behavior_dev == boot_binding.behavior_dev) {
        return 0; /* the reboot is already scheduled: swallow the key release and repeats */
    }
    if (pressed && src_binding) {
        uint32_t wait = keyfx_boot_check(src_binding, &event);

        if (wait) {
            boot_binding = *src_binding;
            boot_event = event;
            boot_pending = true;
            k_work_schedule(&boot_work, K_MSEC(wait));
            return 0;
        }
        keyfx_on_binding(src_binding, &event);
    }
    return __real_zmk_behavior_invoke_binding(src_binding, event, pressed);
}
