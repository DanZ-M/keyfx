/*
 * &<wrapper> behavior (zmk,behavior-keyfx-wrap): plays a named light effect, then runs
 * the wrapped binding with the same press/release. Because it sits in the action's
 * own binding, the effect starts exactly when ZMK runs the combo, macro or behavior.
 */

#define DT_DRV_COMPAT zmk_behavior_keyfx_wrap

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "keyfx_internal.h"

LOG_MODULE_DECLARE(keyfx, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct wrap_config {
    struct zmk_behavior_binding binding;
    const char *fx;
    uint8_t origin;
};

static int wrap_init(const struct device *dev) {
    return 0;
}

static int wrap_run(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event,
                    bool pressed) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct wrap_config *cfg = dev ? dev->config : NULL;

    if (!cfg) {
        return -ENODEV;
    }

    if (pressed) {
        uint32_t pos = cfg->origin < 255 ? cfg->origin : event.position;
        keyfx_play_by_name(cfg->fx, pos < 255 ? pos : 0);
    }

    struct zmk_behavior_binding inner = cfg->binding;
    return zmk_behavior_invoke_binding(&inner, event, pressed);
}

static int wrap_pressed(struct zmk_behavior_binding *binding,
                        struct zmk_behavior_binding_event event) {
    return wrap_run(binding, event, true);
}

static int wrap_released(struct zmk_behavior_binding *binding,
                         struct zmk_behavior_binding_event event) {
    return wrap_run(binding, event, false);
}

static const struct behavior_driver_api wrap_api = {
    .binding_pressed = wrap_pressed,
    .binding_released = wrap_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};

#define WRAP_PARAM(n, cell)                                                                        \
    COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(DT_DRV_INST(n), bindings, 0, cell), (0),                    \
                (DT_INST_PHA_BY_IDX(n, bindings, 0, cell)))

#define WRAP_INST(n)                                                                               \
    static const struct wrap_config wrap_cfg_##n = {                                               \
        .binding =                                                                                 \
            {                                                                                      \
                .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),            \
                .param1 = WRAP_PARAM(n, param1),                                                   \
                .param2 = WRAP_PARAM(n, param2),                                                   \
            },                                                                                     \
        .fx = DT_INST_PROP(n, fx_name),                                                            \
        .origin = DT_INST_PROP(n, origin_position),                                                \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, wrap_init, NULL, NULL, &wrap_cfg_##n, POST_KERNEL,                  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &wrap_api);

DT_INST_FOREACH_STATUS_OKAY(WRAP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
