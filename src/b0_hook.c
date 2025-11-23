/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/linker/devicetree_regions.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <flash_map_pm.h>
#include <fw_info_bare.h>
#include "btldr_img_op.h"
#include "b0_button.h"
#include "b0_led.h"
#include "b0_segger_rtt.h"
#include "b0_led_err.h"
#include "b0_sleep.h"
#include "ruuvi_fa_id.h"
#include "app_version.h"
#include "ncs_version.h"
#include "ncs_commit.h"
#include "version.h"
#include "zephyr_commit.h"
#include "zephyr_api.h"

LOG_MODULE_REGISTER(B0, LOG_LEVEL_INF);

#define BOOT_MODE_TYPE_FACTORY_RESET (0xAC)

#define DELAY_ACTIVATE_FACTORY_RECOVERY_MS (10 * 1000)

#define SHARED_NODE DT_NODELABEL(shared_sram)

_Static_assert(PM_B0_SIZE == PM_B0_EXT_SIZE, "b0 size must be equal to b0_ext size");
_Static_assert(PM_PROVISION_SIZE == PM_PROVISION_EXT_SIZE, "provision size must be equal to provision_ext size");
_Static_assert(PM_S0_SIZE == PM_S0_EXT_SIZE, "s0 size must be equal to s0_ext size");
_Static_assert(PM_S1_SIZE == PM_S1_EXT_SIZE, "s1 size must be equal to s1_ext size");
_Static_assert(
    PM_MCUBOOT_PRIMARY_SIZE == PM_MCUBOOT_PRIMARY_EXT_SIZE,
    "mcuboot_primary size must be equal to mcuboot_primary_ext size");
_Static_assert(
    PM_MCUBOOT_SECONDARY_SIZE == PM_MCUBOOT_SECONDARY_EXT_SIZE,
    "mcuboot_secondary size must be equal to mcuboot_secondary_ext size");

_Static_assert(PM_S0_SIZE == PM_S1_SIZE, "PM_S0_SIZE must be equal to PM_S1_SIZE");
/* MCUboot can call crypto functions shared by B0 and this reserved memory area
 * will be used to pass content of MCUboot firmware image for checking B0 signature. */
static __aligned(4) __attribute__((used)) volatile uint8_t
    g_reserved_mem[MAX(PM_S0_SIZE, PM_S1_SIZE)] Z_GENERIC_SECTION(LINKER_DT_NODE_REGION_NAME(SHARED_NODE));

__NO_RETURN void
on_factory_fw_recovery_fail(void)
{
    LOG_ERR("B0: Factory fw recovery failed");
    LOG_INF("B0: Wait until button is released");
    (void)arch_irq_lock();
    b0_led_stop_blinking();
    b0_led_err_blink_red_led(NUM_RED_LED_BLINKS_ON_FW_RECOVERY_FAIL);
}

static bool
check_and_handle_button_press(bool* const p_flag_activate_fw_loader)
{
    *p_flag_activate_fw_loader = false;
    if (!b0_button_get())
    {
        return false;
    }
    b0_led_red_and_green_on();
    LOG_INF(
        "B0: Wait %u seconds to activate factory fw recovery (while button is pressed)",
        DELAY_ACTIVATE_FACTORY_RECOVERY_MS / 1000);
    const uint32_t timestamp = k_uptime_get_32();
    for (;;)
    {
        const bool button_state = b0_button_get();
        if (!button_state)
        {
            LOG_INF("B0: Button released");
            b0_led_red_and_green_off();
            *p_flag_activate_fw_loader = true;
            return false;
        }

        const uint32_t delta = k_uptime_get_32() - timestamp;

        if (delta >= DELAY_ACTIVATE_FACTORY_RECOVERY_MS)
        {
            break;
        }
    }
    b0_led_red_and_green_off();
    return true;
}

static bool
check_img_in_ext_flash(const fa_id_t fa_id, const char* fa_name)
{
    static uint8_t           img_header_buf[FW_INFO_OFFSET4 + sizeof(struct fw_info)];
    const struct flash_area* p_fa = NULL;
    int32_t                  rc   = flash_area_open(fa_id, &p_fa);
    if (0 != rc)
    {
        LOG_ERR("Failed to open flash area %d (%s), rc=%d", fa_id, fa_name, rc);
        return false;
    }
    rc = flash_area_read(p_fa, (off_t)0, img_header_buf, sizeof(img_header_buf));
    if (rc != 0)
    {
        LOG_ERR(
            "Failed to read flash area %d (%s), address 0x%08x, size=%u, rc=%d",
            fa_id,
            fa_name,
            (unsigned)p_fa->fa_off,
            sizeof(img_header_buf),
            rc);
        return false;
    }
    const struct fw_info* const p_img_info = fw_info_find((uint32_t)img_header_buf);
    if (NULL == p_img_info)
    {
        LOG_ERR("Failed to find fw_info for image in flash area %d (%s)", fa_id, fa_name);
        return false;
    }
    LOG_INF("Check image in ext flash area %d (%s): OK", fa_id, fa_name);
    return true;
}

static bool
check_images_in_ext_flash(void)
{
    if (!check_img_in_ext_flash(FIXED_PARTITION_ID(s0_ext), "s0_ext"))
    {
        return false;
    }
    if (!check_img_in_ext_flash(FIXED_PARTITION_ID(s1_ext), "s1_ext"))
    {
        return false;
    }
    if (!check_img_in_ext_flash(FIXED_PARTITION_ID(mcuboot_primary_ext), "mcuboot_primary_ext"))
    {
        return false;
    }
    if (!check_img_in_ext_flash(FIXED_PARTITION_ID(mcuboot_secondary_ext), "mcuboot_secondary_ext"))
    {
        return false;
    }
    return true;
}

static bool
copy_img_from_ext_flash_to_int_flash(
    const fa_id_t     fa_id_src,
    const char* const p_fa_src_name,
    const fa_id_t     fa_id_dst,
    const char* const p_fa_dst_name)
{
    LOG_INF(
        "B0: Copy image from external flash to internal flash: %d (%s) -> %d (%s)",
        fa_id_src,
        p_fa_src_name,
        fa_id_dst,
        p_fa_dst_name);

    btldr_img_op_copy(fa_id_dst, fa_id_src);

    if (!btldr_img_op_cmp(fa_id_dst, fa_id_src))
    {
        LOG_ERR(
            "B0: Verification failed after copying image from external flash to internal flash for %s",
            p_fa_dst_name);
        return false;
    }
    return true;
}

static bool
flash_erase(const fa_id_t fa_id, const char* const p_fa_name)
{
    LOG_INF("Erase flash area: %d (%s)", fa_id, p_fa_name);

    const struct flash_area* p_fa = NULL;
    int32_t                  rc   = flash_area_open(fa_id, &p_fa);
    if (rc < 0)
    {
        LOG_ERR("FAIL: unable to find flash area %d: %d", fa_id, rc);
        return false;
    }

    LOG_INF(
        "Erase flash area %d at 0x%08" PRIx32 " (%s), size %zu bytes",
        fa_id,
        (uint32_t)p_fa->fa_off,
        p_fa->fa_dev->name,
        p_fa->fa_size);

    rc = flash_area_flatten(p_fa, 0, p_fa->fa_size);
    if (rc < 0)
    {
        LOG_ERR("Erasing flash area %d failed, rc=%d", fa_id, rc);
        flash_area_close(p_fa);
        return false;
    }
    LOG_INF("Erasing flash area %d finished successfully", fa_id);

    flash_area_close(p_fa);
    return true;
}

static __NO_RETURN void
factory_fw_recovery(void)
{
    b0_led_start_blinking_red_green_500ms();

    if (!check_images_in_ext_flash())
    {
        on_factory_fw_recovery_fail();
    }
    if (!copy_img_from_ext_flash_to_int_flash(
            FIXED_PARTITION_ID(provision_ext),
            "provision_ext",
            FIXED_PARTITION_ID(provision),
            "provision"))
    {
        on_factory_fw_recovery_fail();
    }
    if (!copy_img_from_ext_flash_to_int_flash(FIXED_PARTITION_ID(s0_ext), "s0_ext", FIXED_PARTITION_ID(s0), "s0"))
    {
        on_factory_fw_recovery_fail();
    }
    if (!copy_img_from_ext_flash_to_int_flash(FIXED_PARTITION_ID(s1_ext), "s1_ext", FIXED_PARTITION_ID(s1), "s1"))
    {
        on_factory_fw_recovery_fail();
    }
    if (!copy_img_from_ext_flash_to_int_flash(
            FIXED_PARTITION_ID(mcuboot_primary_ext),
            "mcuboot_primary_ext",
            FIXED_PARTITION_ID(mcuboot_primary),
            "mcuboot_primary"))
    {
        on_factory_fw_recovery_fail();
    }
    if (!copy_img_from_ext_flash_to_int_flash(
            FIXED_PARTITION_ID(mcuboot_secondary_ext),
            "mcuboot_secondary_ext",
            FIXED_PARTITION_ID(mcuboot_secondary),
            "mcuboot_secondary"))
    {
        on_factory_fw_recovery_fail();
    }
    if (!flash_erase(PM_ID(ext_flash_userspace), "ext_flash_userspace"))
    {
        on_factory_fw_recovery_fail();
    }
    LOG_INF("B0: Factory firmware recovered successfully");

    zephyr_api_ret_t rc = bootmode_set(BOOT_MODE_TYPE_FACTORY_RESET);
    if (0 != rc)
    {
        LOG_ERR("bootmode_set failed, rc=%d", rc);
        on_factory_fw_recovery_fail();
    }

    b0_led_stop_blinking();

    if (b0_button_get())
    {
        LOG_INF("B0: Wait until button is released to reboot");
        b0_led_red_and_green_on();
        while (b0_button_get())
        {
            b0_sleep_ms(10); // NOSONAR
        }
        b0_led_red_and_green_off();
    }

    LOG_INF("B0: Rebooting...");
    b0_sleep_ms(500); // NOSONAR
    sys_reboot(SYS_REBOOT_COLD);
}

int // NOSONAR: Zephyr API
soc_late_init_hook(void)
{
    LOG_INF("### B0: Version: %s (FwInfoCnt: %u)", APP_VERSION_EXTENDED_STRING, CONFIG_FW_INFO_FIRMWARE_VERSION);
    LOG_INF("### B0: Build: %s", STRINGIFY(APP_BUILD_VERSION));
    LOG_INF(
        "### B0: NCS version: %s, build: %s, commit: %s",
        NCS_VERSION_STRING,
        STRINGIFY(NCS_BUILD_VERSION),
        NCS_COMMIT_STRING);
    LOG_INF(
        "### B0: Kernel version: %s, build: %s, commit: %s",
        KERNEL_VERSION_EXTENDED_STRING,
        STRINGIFY(BUILD_VERSION),
        ZEPHYR_COMMIT_STRING);

    b0_button_init();

    b0_segger_rtt_check_data_location_and_size();

    bool flag_activate_fw_loader = false;
    if (check_and_handle_button_press(&flag_activate_fw_loader))
    {
        LOG_INF("B0: Activate factory fw recovery mode");
        factory_fw_recovery();
    }
    if (flag_activate_fw_loader)
    {
        LOG_INF("B0: Activate fw_loader mode");
        zephyr_api_ret_t rc = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
        if (0 != rc)
        {
            LOG_ERR("bootmode_set failed, rc=%d", rc);
        }
    }

    LOG_INF("B0: Protect B0 flash at address 0x%08x, size 0x%x bytes", PM_B0_ADDRESS, PM_B0_SIZE);
    LOG_INF("B0: Protect provision area at address 0x%08x, size 0x%x bytes", PM_PROVISION_ADDRESS, PM_PROVISION_SIZE);
    /* The protection will be performed by the main function in ~/ncs/<VERSION>/nrf/samples/bootloader/src/main.c */

#if defined(CONFIG_USE_SEGGER_RTT)
    b0_sleep_ms(500); // NOSONAR
#endif

    return 0;
}
