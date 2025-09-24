/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#if !defined(B0_LED_H)
#define B0_LED_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
b0_led_init(void);

void
b0_led_deinit(void);

void
b0_led_red_set(const bool is_on);

static inline void
b0_led_red_on(void)
{
    b0_led_red_set(true);
}

static inline void
b0_led_red_off(void)
{
    b0_led_red_set(false);
}

void
b0_led_green_set(const bool is_on);

static inline void
b0_led_green_on(void)
{
    b0_led_green_set(true);
}

static inline void
b0_led_green_off(void)
{
    b0_led_green_set(false);
}

static inline void
b0_led_red_and_green_on(void)
{
    b0_led_red_set(true);
    b0_led_green_set(true);
}

static inline void
b0_led_red_and_green_off(void)
{
    b0_led_red_set(false);
    b0_led_green_set(false);
}

void
b0_led_start_blinking_red_green_500ms(void);

void
b0_led_stop_blinking(void);

#ifdef __cplusplus
}
#endif

#endif // B0_LED_H
