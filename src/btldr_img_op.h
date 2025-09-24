/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef BTLDR_IMG_OP_H
#define BTLDR_IMG_OP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
btldr_img_op_copy(const int fa_id_dst, const int fa_id_src);

bool
btldr_img_op_cmp(const int fa_id_dst, const int fa_id_src);

#ifdef __cplusplus
}
#endif

#endif // BTLDR_IMG_OP_H
