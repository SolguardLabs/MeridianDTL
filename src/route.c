#include "meridian/route.h"

#include <stdio.h>
#include <string.h>

const char *md_lane_class_name(MdLaneClass lane_class) {
    switch (lane_class) {
    case MD_LANE_STANDARD:
        return "standard";
    case MD_LANE_PRIORITY:
        return "priority";
    case MD_LANE_REBALANCE:
        return "rebalance";
    case MD_LANE_RECOVERY:
        return "recovery";
    default:
        return "unknown";
    }
}

void md_plan_init(MdSettlementPlan *plan) {
    if (plan == 0) {
        return;
    }
    memset(plan, 0, sizeof(*plan));
    md_digest_to_hex(&plan->settlement_lane);
    md_digest_to_hex(&plan->beneficiary);
    md_digest_to_hex(&plan->operator_account);
    md_digest_to_hex(&plan->memo);
}

MdStatus md_plan_add_hop(MdSettlementPlan *plan, MdRouteHop hop) {
    if (plan == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (plan->hop_count >= MD_MAX_ROUTE_HOPS) {
        return MD_ERR_CAPACITY;
    }
    plan->hops[plan->hop_count] = hop;
    plan->hop_count++;
    return MD_OK;
}

MdDigest md_route_hop_digest(const MdRouteHop *hop) {
    MdHash hash;

    md_hash_init(&hash, "meridian-route-hop");
    if (hop != 0) {
        md_hash_update(&hash, hop->lane_id.bytes, sizeof(hop->lane_id.bytes));
        md_hash_update(&hash, hop->from_vault.bytes, sizeof(hop->from_vault.bytes));
        md_hash_update(&hash, hop->to_vault.bytes, sizeof(hop->to_vault.bytes));
        md_hash_update(&hash, hop->bridge_account.bytes, sizeof(hop->bridge_account.bytes));
        md_hash_update(&hash, &hop->amount_hint, sizeof(hop->amount_hint));
        md_hash_update(&hash, &hop->weight_bps, sizeof(hop->weight_bps));
        md_hash_update(&hash, &hop->priority, sizeof(hop->priority));
    }
    return md_hash_finish(&hash);
}

MdStatus md_plan_canonical(const MdSettlementPlan *plan, char *out, size_t out_len) {
    MdWriter writer;

    if (plan == 0 || out == 0 || out_len == 0u) {
        return MD_ERR_ARGUMENT;
    }
    md_writer_init(&writer, out, out_len);
    md_writer_put(&writer, "plan{");
    md_writer_digest(&writer, "lane", &plan->settlement_lane);
    md_writer_digest(&writer, "beneficiary", &plan->beneficiary);
    md_writer_digest(&writer, "operator", &plan->operator_account);
    md_writer_digest(&writer, "memo", &plan->memo);
    md_writer_amount(&writer, "min_net", plan->min_net_amount);
    md_writer_u32(&writer, "max_fee_bps", plan->max_fee_bps);
    md_writer_u64(&writer, "quote_epoch", plan->quote_epoch);
    md_writer_u64(&writer, "settle_after", plan->settle_after_epoch);
    md_writer_u64(&writer, "hop_count", (uint64_t)plan->hop_count);
    for (size_t i = 0u; i < plan->hop_count; i++) {
        MdDigest hop_digest = md_route_hop_digest(&plan->hops[i]);
        md_writer_put(&writer, "hop{");
        md_writer_u64(&writer, "idx", (uint64_t)i);
        md_writer_digest(&writer, "digest", &hop_digest);
        md_writer_digest(&writer, "lane", &plan->hops[i].lane_id);
        md_writer_digest(&writer, "from", &plan->hops[i].from_vault);
        md_writer_digest(&writer, "to", &plan->hops[i].to_vault);
        md_writer_digest(&writer, "bridge", &plan->hops[i].bridge_account);
        md_writer_amount(&writer, "amount_hint", plan->hops[i].amount_hint);
        md_writer_u32(&writer, "weight_bps", plan->hops[i].weight_bps);
        md_writer_u32(&writer, "priority", plan->hops[i].priority);
        md_writer_put(&writer, "}");
    }
    md_writer_put(&writer, "}");
    return md_writer_status(&writer);
}

MdDigest md_plan_digest(const MdSettlementPlan *plan) {
    char payload[4096];

    if (md_plan_canonical(plan, payload, sizeof(payload)) != MD_OK) {
        return md_digest_text("meridian-route-plan", "invalid-plan");
    }
    return md_digest_canonical("meridian-route-plan", payload);
}

static int md_hops_compatible_for_compaction(const MdRouteHop *left, const MdRouteHop *right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    if (!md_digest_equal(&left->lane_id, &right->lane_id)) {
        return 0;
    }
    if (!md_digest_equal(&left->to_vault, &right->from_vault)) {
        return 0;
    }
    if (!md_digest_equal(&left->bridge_account, &right->bridge_account)) {
        return 0;
    }
    return 1;
}

MdStatus md_plan_compact(const MdSettlementPlan *plan, MdCompactedPlan *out) {
    MdSettlementPlan compacted;
    size_t removed = 0u;

    if (plan == 0 || out == 0) {
        return MD_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->route_digest_before = md_plan_digest(plan);
    compacted = *plan;
    compacted.hop_count = 0u;

    for (size_t i = 0u; i < plan->hop_count; i++) {
        if (compacted.hop_count > 0u) {
            MdRouteHop *prev = &compacted.hops[compacted.hop_count - 1u];
            const MdRouteHop *next = &plan->hops[i];
            if (md_hops_compatible_for_compaction(prev, next)) {
                prev->to_vault = next->to_vault;
                prev->amount_hint = next->amount_hint;
                if (next->priority > prev->priority) {
                    prev->priority = next->priority;
                }
                removed++;
                continue;
            }
        }
        compacted.hops[compacted.hop_count] = plan->hops[i];
        compacted.hop_count++;
    }

    if (compacted.hop_count > 0u) {
        compacted.settlement_lane = compacted.hops[compacted.hop_count - 1u].lane_id;
    }
    out->plan = compacted;
    out->removed_hops = removed;
    out->effective_lane = compacted.settlement_lane;
    out->effective_beneficiary = compacted.beneficiary;
    out->route_digest_after = md_plan_digest(&compacted);
    return MD_OK;
}
