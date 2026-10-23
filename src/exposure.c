#include "meridian/exposure.h"

static uint32_t md_utilization_bps(MdAmount amount, MdAmount capacity) {
    MdAmount remainder = 0u;
    uint32_t result = 0u;
    uint32_t index;

    if (capacity == 0u || amount == 0u) {
        return 0u;
    }
    if (amount >= capacity) {
        return MD_BPS_DENOMINATOR;
    }
    for (index = 0u; index < MD_BPS_DENOMINATOR; index++) {
        if (remainder >= capacity - amount) {
            remainder -= capacity - amount;
            result++;
        } else {
            remainder += amount;
        }
    }
    return result;
}

MdStatus md_route_exposure_charge(const MdLedger *ledger, const MdCompactedPlan *route, MdAmount *out) {
    MdAmount total = 0u;
    size_t index;

    if (ledger == 0 || route == 0 || out == 0 || route->plan.hop_count > MD_MAX_ROUTE_HOPS) {
        return MD_ERR_ARGUMENT;
    }
    for (index = 0u; index < route->plan.hop_count; index++) {
        const MdRouteHop *hop = &route->plan.hops[index];
        const MdVault *source = md_ledger_find_vault_const(ledger, &hop->from_vault);
        MdAmount weighted;
        MdAmount charge;
        uint32_t utilization;

        if (source == 0 || !source->active || source->balance == 0u ||
            hop->amount_hint > source->balance || hop->weight_bps > MD_BPS_DENOMINATOR) {
            return MD_ERR_POLICY;
        }
        utilization = md_utilization_bps(hop->amount_hint, source->balance);
        if (md_amount_mul_bps(hop->amount_hint, hop->weight_bps, &weighted) != MD_OK ||
            md_amount_mul_bps(weighted, utilization, &charge) != MD_OK ||
            md_amount_add(total, charge, &total) != MD_OK) {
            return MD_ERR_POLICY;
        }
    }
    *out = total;
    return MD_OK;
}
