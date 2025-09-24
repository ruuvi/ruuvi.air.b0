/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef B0_BUTTON_H
#define B0_BUTTON_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
b0_button_init(void);

void
b0_button_deinit(void);

bool
b0_button_get(void);

#ifdef __cplusplus
}
#endif

#endif // B0_BUTTON_H
