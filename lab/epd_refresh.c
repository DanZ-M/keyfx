/*
 * keyfx dongle lab: e-paper refresh policy.
 *
 * With a `partial` refresh profile in the devicetree the panel updates only what changed
 * (fast, no flicker). Each partial update leaves a little ghosting behind, so this file adds
 * a full refresh of the same picture (the black-white-black flash) when the screen has been
 * quiet for a while, or after a number of changes. It runs on ZMK's display work queue, the
 * same thread that draws, so nothing else touches the panel at the same time.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/atomic.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>

static const struct device *const epd = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static atomic_t changes;
static struct k_work_delayable full_work;

static void full_refresh(struct k_work *work)
{
	if (atomic_set(&changes, 0) == 0 || !zmk_display_is_initialized() || !device_is_ready(epd)) {
		return;
	}
	/* blanking on = the driver switches to the full profile and holds updates; redraw the whole
	   screen into the panel memory; blanking off = one full refresh of that picture */
	display_blanking_on(epd);
	lv_obj_invalidate(lv_screen_active());
	lv_refr_now(NULL);
	display_blanking_off(epd);
}

static int epd_listener(const zmk_event_t *eh)
{
	int n = atomic_inc(&changes) + 1;
	k_timeout_t when = K_SECONDS(CONFIG_KEYFX_LAB_EPD_FULL_IDLE_S);

#if CONFIG_KEYFX_LAB_EPD_FULL_AFTER_CHANGES > 0
	if (n >= CONFIG_KEYFX_LAB_EPD_FULL_AFTER_CHANGES) {
		when = K_SECONDS(3); /* let the last change settle, then clean up */
	}
#endif
	k_work_reschedule_for_queue(zmk_display_work_q(), &full_work, when);
	return ZMK_EV_EVENT_BUBBLE;
}

static int epd_init(void)
{
	k_work_init_delayable(&full_work, full_refresh);
	return 0;
}
SYS_INIT(epd_init, APPLICATION, 90);

ZMK_LISTENER(keyfx_epd, epd_listener);
ZMK_SUBSCRIPTION(keyfx_epd, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(keyfx_epd, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(keyfx_epd, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(keyfx_epd, zmk_battery_state_changed);
