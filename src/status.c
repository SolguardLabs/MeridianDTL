#include "meridian/status.h"

const char *md_status_name(MdStatus status) {
    switch (status) {
    case MD_OK:
        return "ok";
    case MD_ERR_ARGUMENT:
        return "argument";
    case MD_ERR_CAPACITY:
        return "capacity";
    case MD_ERR_NOT_FOUND:
        return "not_found";
    case MD_ERR_DUPLICATE:
        return "duplicate";
    case MD_ERR_SIGNATURE:
        return "signature";
    case MD_ERR_BALANCE:
        return "balance";
    case MD_ERR_POLICY:
        return "policy";
    case MD_ERR_STATE:
        return "state";
    case MD_ERR_EXPIRED:
        return "expired";
    case MD_ERR_INTERNAL:
        return "internal";
    default:
        return "unknown";
    }
}
