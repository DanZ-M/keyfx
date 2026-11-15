#pragma once
#include <stdint.h>

/* op codes, carried in the top byte of the behavior's first parameter */
#define KEYFX_OP_CMD   0x00 /* user command from a key (&kfx KFX_xxx n) */
#define KEYFX_OP_STATE 0x01 /* central -> peripheral: layers, mods, flags */
#define KEYFX_OP_FX    0x02 /* central -> peripheral: start an effect     */

/* Called by the behavior driver on whichever half receives the invocation. */
void keyfx_engine_command(uint32_t param1, uint32_t param2);

/* Central only: run the behavior on the peripheral (and locally, where it is ignored). */
void keyfx_sync_send(uint32_t param1, uint32_t param2);

/* Start the effect named in CI_EFFECT (central only; ignored elsewhere). `origin_pos` is
 * the key position the effect starts from (0-41). */
void keyfx_play_by_name(const char *name, uint8_t origin_pos);

struct zmk_behavior_binding;
struct zmk_behavior_binding_event;

/* Central only: called for every behavior ZMK runs (through the link-time hook). */
void keyfx_on_binding(const struct zmk_behavior_binding *binding,
                     const struct zmk_behavior_binding_event *event);
