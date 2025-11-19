/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "b0_supercap.h"
#include "b0_led.h"
#include "b0_ext_flash_power.h"

#define CONFIG_RUUVI_AIR_GPIO_EXT_FLASH_POWER_ON_PRIORITY 41
_Static_assert(CONFIG_RUUVI_AIR_GPIO_EXT_FLASH_POWER_ON_PRIORITY > CONFIG_GPIO_INIT_PRIORITY);
_Static_assert(CONFIG_RUUVI_AIR_GPIO_EXT_FLASH_POWER_ON_PRIORITY < CONFIG_NORDIC_QSPI_NOR_INIT_PRIORITY);

static int // NOSONAR: Zephyr init functions must return int
b0_early_init(void)
{
    printk("\r\n*** Ruuvi B0 Bootloader ***\r\n");
#if defined(CONFIG_BOARD_RUUVI_RUUVIAIR_REV_1)
    b0_supercap_init();
#endif // CONFIG_BOARD_RUUVI_RUUVIAIR_REV_1
    b0_led_init();
    b0_ext_flash_power_on(); // Turn on power to external flash memory and sensors
    return 0;
}

SYS_INIT(b0_early_init, POST_KERNEL, CONFIG_RUUVI_AIR_GPIO_EXT_FLASH_POWER_ON_PRIORITY);
