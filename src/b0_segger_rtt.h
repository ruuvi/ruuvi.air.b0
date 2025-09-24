/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#if !defined(B0_SEGGER_RTT_H)
#define B0_SEGGER_RTT_H

#ifdef __cplusplus
extern "C" {
#endif

void
b0_segger_rtt_check_data_location_and_size(void);

void
b0_segger_rtt_write(const void* p_buffer, const unsigned len);

#ifdef __cplusplus
}
#endif

#endif // B0_SEGGER_RTT_H
