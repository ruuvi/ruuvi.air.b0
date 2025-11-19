/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "b0_led_err.h"
#include <stdint.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include "b0_led.h"
#include "b0_button.h"
#include "b0_sleep.h"

LOG_MODULE_DECLARE(B0, LOG_LEVEL_INF);

#define DELAY_BEFORE_REBOOT_MS       100
#define LED_FLASH_DURATION_MS        100
#define BUTTON_PRESS_CHECK_PERIOD_MS 100
#define DELAY_BETWEEN_BLINKS_MS      900

static bool
check_if_button_released_and_pressed(bool* p_is_button_released)
{
    if (!*p_is_button_released)
    {
        if (!b0_button_get())
        {
            *p_is_button_released = true;
            LOG_INF("B0: Button is released");
            LOG_INF("B0: Wait until button is pressed to reboot");
        }
    }
    else
    {
        if (b0_button_get())
        {
            LOG_INF("B0: Button is pressed - reboot");
            b0_sleep_ms(DELAY_BEFORE_REBOOT_MS);
            return true;
        }
    }
    return false;
}

__NO_RETURN void
b0_led_err_blink_red_led(const uint32_t num_red_blinks)
{
    bool is_button_released = !b0_button_get();
    if (is_button_released)
    {
        LOG_INF("B0: Wait until button is pressed to reboot");
    }
    b0_led_green_off();
    while (1)
    {
        for (uint32_t i = 0; i < num_red_blinks; ++i)
        {
            b0_led_red_on();
            b0_sleep_ms(LED_FLASH_DURATION_MS);
            if (check_if_button_released_and_pressed(&is_button_released))
            {
                sys_reboot(SYS_REBOOT_COLD);
            }
            b0_led_red_off();
            b0_sleep_ms(BUTTON_PRESS_CHECK_PERIOD_MS);
            if (check_if_button_released_and_pressed(&is_button_released))
            {
                sys_reboot(SYS_REBOOT_COLD);
            }
        }
        for (int32_t i = 0; i < (DELAY_BETWEEN_BLINKS_MS / BUTTON_PRESS_CHECK_PERIOD_MS); ++i) // NOSONAR
        {
            b0_sleep_ms(BUTTON_PRESS_CHECK_PERIOD_MS);
            if (check_if_button_released_and_pressed(&is_button_released))
            {
                sys_reboot(SYS_REBOOT_COLD);
            }
        }
    }
}
