#ifndef MERIDIAN_AMOUNT_H
#define MERIDIAN_AMOUNT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t MdAmount;

#define MD_BPS_DENOMINATOR 10000u

MdStatus md_amount_add(MdAmount left, MdAmount right, MdAmount *out);
MdStatus md_amount_sub(MdAmount left, MdAmount right, MdAmount *out);
MdStatus md_amount_mul_bps(MdAmount amount, uint32_t bps, MdAmount *out);
MdStatus md_amount_to_string(MdAmount amount, char *out, size_t out_len);
bool md_amount_is_zero(MdAmount amount);

#ifdef __cplusplus
}
#endif

#endif
