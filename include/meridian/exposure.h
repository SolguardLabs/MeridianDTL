#ifndef MERIDIAN_EXPOSURE_H
#define MERIDIAN_EXPOSURE_H

#include "meridian/ledger.h"

#ifdef __cplusplus
extern "C" {
#endif

MdStatus md_route_exposure_charge(const MdLedger *ledger, const MdCompactedPlan *route, MdAmount *out);

#ifdef __cplusplus
}
#endif

#endif
