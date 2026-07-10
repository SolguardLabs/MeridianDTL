#ifndef MERIDIAN_ROUTE_H
#define MERIDIAN_ROUTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meridian/amount.h"
#include "meridian/codec.h"
#include "meridian/digest.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_MAX_ROUTE_HOPS 8u

typedef enum MdLaneClass {
    MD_LANE_STANDARD = 0,
    MD_LANE_PRIORITY = 1,
    MD_LANE_REBALANCE = 2,
    MD_LANE_RECOVERY = 3
} MdLaneClass;

const char *md_lane_class_name(MdLaneClass lane_class);

typedef struct MdRouteHop {
    MdDigest lane_id;
    MdDigest from_vault;
    MdDigest to_vault;
    MdDigest bridge_account;
    MdAmount amount_hint;
    uint32_t weight_bps;
    uint32_t priority;
} MdRouteHop;

typedef struct MdSettlementPlan {
    MdDigest settlement_lane;
    MdDigest beneficiary;
    MdDigest operator_account;
    MdDigest memo;
    MdRouteHop hops[MD_MAX_ROUTE_HOPS];
    size_t hop_count;
    MdAmount min_net_amount;
    uint32_t max_fee_bps;
    uint64_t quote_epoch;
    uint64_t settle_after_epoch;
} MdSettlementPlan;

typedef struct MdCompactedPlan {
    MdSettlementPlan plan;
    MdDigest route_digest_before;
    MdDigest route_digest_after;
    MdDigest effective_beneficiary;
    MdDigest effective_lane;
    size_t removed_hops;
} MdCompactedPlan;

void md_plan_init(MdSettlementPlan *plan);
MdStatus md_plan_add_hop(MdSettlementPlan *plan, MdRouteHop hop);
MdDigest md_route_hop_digest(const MdRouteHop *hop);
MdDigest md_plan_digest(const MdSettlementPlan *plan);
MdStatus md_plan_canonical(const MdSettlementPlan *plan, char *out, size_t out_len);
MdStatus md_plan_compact(const MdSettlementPlan *plan, MdCompactedPlan *out);

#ifdef __cplusplus
}
#endif

#endif
