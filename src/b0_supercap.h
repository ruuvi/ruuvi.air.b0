/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#if !defined(B0_SUPERCAP_H)
#define B0_SUPERCAP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_BOARD_RUUVI_RUUVIAIR_REV_1)

void
b0_supercap_init(void);

void
b0_supercap_deinit(void);

#endif // CONFIG_BOARD_RUUVI_RUUVIAIR_REV_1

#ifdef __cplusplus
}
#endif

#endif // B0_SUPERCAP_H
