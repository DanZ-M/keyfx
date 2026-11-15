/*
 * &kfx behavior: lets keys control the light effects and carries state from the
 * central half to the peripheral half. It has GLOBAL locality, so ZMK runs it on
 * both halves, over the same channel the RGB underglow commands use.
 */

#define DT_DRV_COMPAT zmk_behavior_keyfx

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "keyfx_internal.h"

LOG_MODULE_DECLARE(keyfx, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int kfx_init(const struct device *dev) {
    return 0;
}

static int kfx_pressed(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    keyfx_engine_command(binding->param1, binding->param2);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int kfx_released(struct zmk_behavior_binding *binding,
                        struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api kfx_api = {
    .binding_pressed = kfx_pressed,
    .binding_released = kfx_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, kfx_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &kfx_api);

void keyfx_sync_send(uint32_t param1, uint32_t param2) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_DRV_INST(0)),
        .param1 = param1,
        .param2 = param2,
    };
    struct zmk_behavior_binding_event event = {
        .layer = 0,
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    int rc = zmk_behavior_invoke_binding(&binding, event, true);
    if (rc < 0) {
        LOG_DBG("sync send failed (%d)", rc);
    }
}

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
