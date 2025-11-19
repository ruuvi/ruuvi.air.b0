/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef BTLDR_IMG_OP_H
#define BTLDR_IMG_OP_H

#include <stdbool.h>
#include "ruuvi_fa_id.h"

#ifdef __cplusplus
extern "C" {
#endif

void
btldr_img_op_copy(const fa_id_t fa_id_dst, const fa_id_t fa_id_src);

bool
btldr_img_op_cmp(const fa_id_t fa_id_dst, const fa_id_t fa_id_src);

#ifdef __cplusplus
}
#endif

#endif // BTLDR_IMG_OP_H
