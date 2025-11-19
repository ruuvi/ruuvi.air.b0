/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "btldr_img_op.h"
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <cmsis_gcc.h>
#include "zephyr_api.h"

LOG_MODULE_DECLARE(B0, LOG_LEVEL_INF);

#define TMP_BUF_SIZE 256

extern __NO_RETURN void
on_factory_fw_recovery_fail(void);

typedef bool (*cb_img_process_t)(
    const struct flash_area* p_fa_dst,
    const off_t              offset,
    const uint8_t*           p_src_img_data_buf,
    const size_t             buf_len);

static bool
img_process(
    const fa_id_t    fa_id_dst,
    const fa_id_t    fa_id_src,
    const bool       flag_erase_dst,
    cb_img_process_t cb_img_process)
{
    static uint8_t tmp_buf1[TMP_BUF_SIZE];

    const struct flash_area* p_fa_dst = NULL;
    const struct flash_area* p_fa_src = NULL;

    int32_t rc = flash_area_open(fa_id_dst, &p_fa_dst);
    if (0 != rc)
    {
        LOG_ERR("Failed to open flash area %d, rc=%d", fa_id_dst, rc);
        on_factory_fw_recovery_fail();
    }

    rc = flash_area_open(fa_id_src, &p_fa_src);
    if (0 != rc)
    {
        LOG_ERR("Failed to open flash area %d, rc=%d", fa_id_src, rc);
        on_factory_fw_recovery_fail();
    }

    if (p_fa_dst->fa_size != p_fa_src->fa_size)
    {
        LOG_ERR("Image size mismatch: %d != %d", p_fa_dst->fa_size, p_fa_src->fa_size);
        on_factory_fw_recovery_fail();
    }

    if (flag_erase_dst)
    {
        rc = flash_area_erase(p_fa_dst, 0, p_fa_dst->fa_size);
        if (rc != 0)
        {
            LOG_ERR(
                "Failed to erase flash area %d (address 0x%08x, size 0x%08x), rc=%d",
                fa_id_dst,
                (unsigned)p_fa_dst->fa_off,
                (unsigned)p_fa_dst->fa_size,
                rc);
            on_factory_fw_recovery_fail();
        }
    }

    bool   is_success = true;
    size_t rem_len    = p_fa_src->fa_size;
    off_t  offset     = 0;
    while (rem_len > 0)
    {
        const size_t len = (rem_len > TMP_BUF_SIZE) ? TMP_BUF_SIZE : rem_len;

        rc = flash_area_read(p_fa_src, offset, tmp_buf1, len);
        if (rc != 0)
        {
            LOG_ERR(
                "Failed to read flash area %d, address 0x%08x, rc=%d",
                fa_id_src,
                (unsigned)(p_fa_src->fa_off + offset),
                rc);
            on_factory_fw_recovery_fail();
        }

        if (!cb_img_process(p_fa_dst, offset, tmp_buf1, len))
        {
            is_success = false;
            break;
        }

        offset += len;
        rem_len -= len;
    }

    flash_area_close(p_fa_src);
    flash_area_close(p_fa_dst);
    return is_success;
}

static bool
cb_img_write(
    const struct flash_area* p_fa_dst,
    const off_t              offset,
    const uint8_t*           p_src_img_data_buf,
    const size_t             buf_len)
{
    zephyr_api_ret_t rc = flash_area_write(p_fa_dst, offset, p_src_img_data_buf, buf_len);
    if (rc != 0)
    {
        LOG_ERR("Failed to write at address 0x%08x, rc=%d", (unsigned)(p_fa_dst->fa_off + offset), rc);
        on_factory_fw_recovery_fail();
    }
    return true;
}

static bool
cb_img_cmp(
    const struct flash_area* p_fa_dst,
    const off_t              offset,
    const uint8_t*           p_src_img_data_buf,
    const size_t             buf_len)
{
    static uint8_t tmp_buf2[TMP_BUF_SIZE];

    zephyr_api_ret_t rc = flash_area_read(p_fa_dst, offset, tmp_buf2, buf_len);
    if (rc != 0)
    {
        LOG_ERR("Failed to read flash at address 0x%08x, rc=%d", (unsigned)(p_fa_dst->fa_off + offset), rc);
        on_factory_fw_recovery_fail();
    }

    if (memcmp(p_src_img_data_buf, tmp_buf2, buf_len) != 0)
    {
        LOG_INF("memcmp failed at address 0x%08x", (unsigned)(p_fa_dst->fa_off + offset));
        LOG_HEXDUMP_DBG(p_src_img_data_buf, buf_len, "src:");
        LOG_HEXDUMP_DBG(tmp_buf2, buf_len, "dst:");
        return false;
    }
    return true;
}

void
btldr_img_op_copy(const fa_id_t fa_id_dst, const fa_id_t fa_id_src)
{
    img_process(fa_id_dst, fa_id_src, true, &cb_img_write);
}

bool
btldr_img_op_cmp(const fa_id_t fa_id_dst, const fa_id_t fa_id_src)
{
    return img_process(fa_id_dst, fa_id_src, false, &cb_img_cmp);
}
