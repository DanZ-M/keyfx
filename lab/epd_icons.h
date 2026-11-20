/* keyfx dongle lab: icons for the e-paper status screen (generated, see epd_icons.c) */
#pragma once
#include <lvgl.h>

extern const lv_image_dsc_t epd_bat_charging; /* 20x38 */
extern const lv_image_dsc_t epd_bat_99; /* 20x38 */
extern const lv_image_dsc_t epd_bat_93; /* 20x38 */
extern const lv_image_dsc_t epd_bat_87; /* 20x38 */
extern const lv_image_dsc_t epd_bat_80; /* 20x38 */
extern const lv_image_dsc_t epd_bat_73; /* 20x38 */
extern const lv_image_dsc_t epd_bat_67; /* 20x38 */
extern const lv_image_dsc_t epd_bat_60; /* 20x38 */
extern const lv_image_dsc_t epd_bat_53; /* 20x38 */
extern const lv_image_dsc_t epd_bat_47; /* 20x38 */
extern const lv_image_dsc_t epd_bat_40; /* 20x38 */
extern const lv_image_dsc_t epd_bat_33; /* 20x38 */
extern const lv_image_dsc_t epd_bat_27; /* 20x38 */
extern const lv_image_dsc_t epd_bat_20; /* 20x38 */
extern const lv_image_dsc_t epd_bat_13; /* 20x38 */
extern const lv_image_dsc_t epd_bat_7; /* 20x38 */
extern const lv_image_dsc_t epd_bat_2; /* 20x38 */
extern const lv_image_dsc_t epd_out_usb; /* 36x38 */
extern const lv_image_dsc_t epd_out_ble_adv; /* 36x38 */
extern const lv_image_dsc_t epd_out_ble_conn; /* 36x38 */
extern const lv_image_dsc_t epd_out_ble_disc; /* 36x38 */
extern const lv_image_dsc_t epd_prof_usb; /* 74x26 */
extern const lv_image_dsc_t epd_prof_unknown; /* 74x26 */
extern const lv_image_dsc_t epd_prof_1; /* 74x26 */
extern const lv_image_dsc_t epd_prof_2; /* 74x26 */
extern const lv_image_dsc_t epd_prof_3; /* 74x26 */
extern const lv_image_dsc_t epd_prof_4; /* 74x26 */
extern const lv_image_dsc_t epd_prof_5; /* 74x26 */
extern const lv_image_dsc_t epd_pet_sit; /* 126x78 */
extern const lv_image_dsc_t epd_pet_walk; /* 126x78 */
extern const lv_image_dsc_t epd_pet_run; /* 126x78 */
extern const lv_image_dsc_t epd_ball_rest; /* 64x64 */
extern const lv_image_dsc_t epd_ball_0; /* 64x64 */
extern const lv_image_dsc_t epd_ball_45; /* 64x64 */
extern const lv_image_dsc_t epd_ball_90; /* 64x64 */
extern const lv_image_dsc_t epd_ball_135; /* 64x64 */
extern const lv_image_dsc_t epd_ball_180; /* 64x64 */
extern const lv_image_dsc_t epd_ball_225; /* 64x64 */
extern const lv_image_dsc_t epd_ball_270; /* 64x64 */
extern const lv_image_dsc_t epd_ball_315; /* 64x64 */
extern const lv_image_dsc_t epd_ball_unknown; /* 64x64 */

/* battery: the icon of the first entry whose minimum level is <= the charge */
extern const lv_image_dsc_t *const epd_bat_level_icon[16];
extern const uint8_t epd_bat_level_min[16];
extern const lv_image_dsc_t *const epd_prof_ble_icon[5]; /* BLE profile 1..5 */
/* ball: [0] at rest, [1..8] rolling toward 0, 45, ... 315 degrees (0 = right, 90 = down) */
extern const lv_image_dsc_t *const epd_ball_icon[9];
