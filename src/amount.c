#include "meridian/amount.h"

#include <inttypes.h>
#include <stdio.h>

MdStatus md_amount_add(MdAmount left, MdAmount right, MdAmount *out) {
    if (out == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (UINT64_MAX - left < right) {
        return MD_ERR_POLICY;
    }
    *out = left + right;
    return MD_OK;
}

MdStatus md_amount_sub(MdAmount left, MdAmount right, MdAmount *out) {
    if (out == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (left < right) {
        return MD_ERR_BALANCE;
    }
    *out = left - right;
    return MD_OK;
}

MdStatus md_amount_mul_bps(MdAmount amount, uint32_t bps, MdAmount *out) {
    if (out == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (bps > 100000u) {
        return MD_ERR_POLICY;
    }
    if (amount != 0u && bps > UINT64_MAX / amount) {
        return MD_ERR_POLICY;
    }
    *out = (amount * (MdAmount)bps) / (MdAmount)MD_BPS_DENOMINATOR;
    return MD_OK;
}

MdStatus md_amount_to_string(MdAmount amount, char *out, size_t out_len) {
    int written;

    if (out == 0 || out_len == 0u) {
        return MD_ERR_ARGUMENT;
    }
    written = snprintf(out, out_len, "%" PRIu64, amount);
    if (written < 0 || (size_t)written >= out_len) {
        return MD_ERR_CAPACITY;
    }
    return MD_OK;
}

bool md_amount_is_zero(MdAmount amount) {
    return amount == 0u;
}
