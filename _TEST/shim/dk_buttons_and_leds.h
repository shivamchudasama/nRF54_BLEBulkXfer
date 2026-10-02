/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for NCS's DK library (dk_buttons_and_leds.h): the button
   part only. dk_buttons_init() is declared only; a test that reaches it defines
   it and keeps the handler, then calls it as the library would on a change. */
#ifndef _SHIM_DK_BUTTONS_AND_LEDS_H
#define _SHIM_DK_BUTTONS_AND_LEDS_H
#include "zephyr_shim.h"
#define DK_BTN1_MSK           (1U << 0)
#define DK_BTN2_MSK           (1U << 1)
#define DK_BTN3_MSK           (1U << 2)
#define DK_BTN4_MSK           (1U << 3)
typedef void (*button_handler_t)(uint32_t button_state, uint32_t has_changed);
extern int dk_buttons_init(button_handler_t button_handler);
#endif //!_SHIM_DK_BUTTONS_AND_LEDS_H
