/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "b0_sleep.h"
#include <zephyr/kernel.h>

void
b0_sleep_ms(uint32_t ms)
{
#ifdef CONFIG_MULTITHREADING
    k_sleep(K_MSEC(ms));
#else
    k_busy_wait(ms * 1000); // NOSONAR
#endif
}
