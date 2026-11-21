/*
 * keyfx dongle lab: status screen for the e-paper, made of the widgets of the old left OLED.
 *
 *   top row   : output (USB / Bluetooth state), profile (USB or BLE 1..5), battery
 *   below     : the layers engraved on a sphere (active one in the dark band), and the dog
 *               (sits / walks / runs by typing speed)
 *
 * Ported from HellTM's custom widgets (MIT) to the LVGL 9 API. The right half's widgets
 * (peripheral status, logos) are not part of it. Two changes for an e-paper panel:
 *  - the pet is a still picture that changes with the typing speed (no animation: every frame
 *    would be a panel refresh);
 *  - a widget redraws only when its picture really changes, so battery reports and WPM
 *    ticks do not refresh the panel.
 * Icons are in epd_icons.c (upright, 2x). Use with CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/wpm_state_changed.h>

#include "epd_icons.h"
#include "epd_refresh.h"

static lv_obj_t *battery_obj;
static lv_obj_t *output_obj;
static lv_obj_t *profile_obj;
static lv_obj_t *layer_obj;
static lv_obj_t *pet_obj;

/* only touch LVGL when the picture changes: a changed picture is a panel refresh */
static void show(lv_obj_t *obj, const lv_image_dsc_t *img)
{
	if (lv_image_get_src(obj) != (const void *)img) {
		epd_refresh_touch(obj); /* where it was ... */
		lv_image_set_src(obj, img);
		epd_refresh_touch(obj); /* ... and where it is now */
		epd_refresh_note();
	}
}

/* ---- battery ---- */

struct battery_state {
	uint8_t level;
	bool usb_present;
};

static void battery_update(struct battery_state s)
{
	if (s.usb_present) {
		show(battery_obj, &epd_bat_charging);
		return;
	}
	for (int i = 0; i < 16; i++) {
		if (s.level >= epd_bat_level_min[i] || i == 15) {
			show(battery_obj, epd_bat_level_icon[i]);
			return;
		}
	}
}

static struct battery_state battery_get_state(const zmk_event_t *eh)
{
	const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

	return (struct battery_state){
		.level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
		.usb_present = zmk_usb_is_powered(),
#endif
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(epd_widget_battery, struct battery_state, battery_update,
			    battery_get_state)
ZMK_SUBSCRIPTION(epd_widget_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(epd_widget_battery, zmk_usb_conn_state_changed);
#endif

/* ---- output (where the keystrokes go) ---- */

struct output_state {
	struct zmk_endpoint_instance selected_endpoint;
	bool profile_connected;
	bool profile_bonded;
};

static void output_update(struct output_state s)
{
	switch (s.selected_endpoint.transport) {
	case ZMK_TRANSPORT_USB:
		show(output_obj, &epd_out_usb);
		break;
	case ZMK_TRANSPORT_BLE:
		if (!s.profile_bonded) {
			show(output_obj, &epd_out_ble_adv);
		} else if (s.profile_connected) {
			show(output_obj, &epd_out_ble_conn);
		} else {
			show(output_obj, &epd_out_ble_disc);
		}
		break;
	}
}

static struct output_state output_get_state(const zmk_event_t *eh)
{
	return (struct output_state){
		.selected_endpoint = zmk_endpoint_get_selected(),
		.profile_connected = zmk_ble_active_profile_is_connected(),
		.profile_bonded = !zmk_ble_active_profile_is_open(),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(epd_widget_output, struct output_state, output_update, output_get_state)
ZMK_SUBSCRIPTION(epd_widget_output, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(epd_widget_output, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(epd_widget_output, zmk_ble_active_profile_changed);
#endif

/* ---- profile (USB or Bluetooth profile 1..5) ---- */

struct profile_state {
	struct zmk_endpoint_instance selected_endpoint;
	uint8_t index;
};

static void profile_update(struct profile_state s)
{
	if (s.selected_endpoint.transport == ZMK_TRANSPORT_USB) {
		show(profile_obj, &epd_prof_usb);
	} else if (s.index < 5) {
		show(profile_obj, epd_prof_ble_icon[s.index]);
	} else {
		show(profile_obj, &epd_prof_unknown);
	}
}

static struct profile_state profile_get_state(const zmk_event_t *eh)
{
	return (struct profile_state){
		.selected_endpoint = zmk_endpoint_get_selected(),
		.index = zmk_ble_active_profile_index(),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(epd_widget_profile, struct profile_state, profile_update,
			    profile_get_state)
ZMK_SUBSCRIPTION(epd_widget_profile, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(epd_widget_profile, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(epd_widget_profile, zmk_ble_active_profile_changed);
#endif

/* ---- layer: the layers engraved on a sphere; the active one faces the viewer ---- */

struct layer_state {
	uint8_t index;
};

static void layer_update(struct layer_state s)
{
	/* layers 0..3 = BASE, NUMB, MOUSE, FUNC; any other layer shows the empty sphere */
	show(layer_obj, s.index < 4 ? epd_sphere_icon[s.index] : &epd_sphere_unknown);
}

static struct layer_state layer_get_state(const zmk_event_t *eh)
{
	return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}

ZMK_DISPLAY_WIDGET_LISTENER(epd_widget_layer, struct layer_state, layer_update, layer_get_state)
ZMK_SUBSCRIPTION(epd_widget_layer, zmk_layer_state_changed);

/* ---- pet (sits, walks or runs by typing speed) ---- */

struct pet_state {
	uint8_t mode; /* 0 sit, 1 walk, 2 run */
};

static void pet_update(struct pet_state s)
{
	show(pet_obj, s.mode == 0 ? &epd_pet_sit : (s.mode == 1 ? &epd_pet_walk : &epd_pet_run));
}

static struct pet_state pet_get_state(const zmk_event_t *eh)
{
	const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
	int wpm = (ev != NULL) ? ev->state : zmk_wpm_get_state();

	return (struct pet_state){
		.mode = wpm < CONFIG_KEYFX_LAB_EPD_PET_WALK_WPM
				? 0
				: (wpm < CONFIG_KEYFX_LAB_EPD_PET_RUN_WPM ? 1 : 2),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(epd_widget_pet, struct pet_state, pet_update, pet_get_state)
ZMK_SUBSCRIPTION(epd_widget_pet, zmk_wpm_state_changed);

/* ---- the screen (200 x 200) ---- */

lv_obj_t *zmk_display_status_screen(void)
{
	lv_obj_t *screen = lv_obj_create(NULL);

	/* black on white, whatever theme is active */
	lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

	output_obj = lv_image_create(screen);
	profile_obj = lv_image_create(screen);
	battery_obj = lv_image_create(screen);
	pet_obj = lv_image_create(screen);
	layer_obj = lv_image_create(screen);

	/* the pictures first (the sizes are needed to place them), then the places */
	epd_widget_output_init();
	epd_widget_profile_init();
	epd_widget_battery_init();
	epd_widget_layer_init();
	epd_widget_pet_init();

	lv_obj_align(output_obj, LV_ALIGN_TOP_LEFT, 8, 8);
	lv_obj_align(profile_obj, LV_ALIGN_TOP_LEFT, 56, 14);
	lv_obj_align(battery_obj, LV_ALIGN_TOP_RIGHT, -8, 8);
	lv_obj_align(layer_obj, LV_ALIGN_TOP_RIGHT, -2, 46);
	lv_obj_align(pet_obj, LV_ALIGN_BOTTOM_LEFT, 8, -8);

	return screen;
}
