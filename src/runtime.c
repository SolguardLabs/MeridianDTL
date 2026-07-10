#include "meridian/runtime.h"

#include "meridian/intent.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MD_NETWORK_ID 7331u

typedef struct ScenarioEnv {
    MdLedger ledger;
    MdDigest asset;
    MdDigest payer;
    MdDigest merchant;
    MdDigest operator_account;
    MdDigest bridge;
    MdDigest counterparty;
    MdDigest vault_north;
    MdDigest vault_relay;
    MdDigest vault_south;
    MdDigest vault_archive;
    MdDigest lane_direct;
    MdDigest lane_priority;
    MdDigest lane_rebalance;
} ScenarioEnv;

static MdStatus require_status(MdStatus status) {
    return status;
}

static MdRouteHop route_hop(const MdDigest *lane, const MdDigest *from, const MdDigest *to, const MdDigest *bridge, MdAmount amount, uint32_t weight_bps, uint32_t priority) {
    MdRouteHop hop;

    memset(&hop, 0, sizeof(hop));
    hop.lane_id = *lane;
    hop.from_vault = *from;
    hop.to_vault = *to;
    hop.bridge_account = *bridge;
    hop.amount_hint = amount;
    hop.weight_bps = weight_bps;
    hop.priority = priority;
    return hop;
}

static MdStatus setup_env(ScenarioEnv *env) {
    MdStatus status;

    if (env == 0) {
        return MD_ERR_ARGUMENT;
    }
    memset(env, 0, sizeof(*env));
    md_ledger_init(&env->ledger, MD_NETWORK_ID);
    status = require_status(md_ledger_add_asset(&env->ledger, "mUSD", 6u, &env->asset));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_account(&env->ledger, "payer-alice", "payer", &env->payer));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_account(&env->ledger, "merchant-orion", "merchant", &env->merchant));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_account(&env->ledger, "operator-meridian", "operator", &env->operator_account));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_account(&env->ledger, "bridge-internal", "bridge", &env->bridge));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_account(&env->ledger, "counterparty-zenith", "counterparty", &env->counterparty));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_vault(&env->ledger, "north-clearing", &env->asset, 500000u, &env->vault_north));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_vault(&env->ledger, "relay-clearing", &env->asset, 250000u, &env->vault_relay));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_vault(&env->ledger, "south-clearing", &env->asset, 100000u, &env->vault_south));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_vault(&env->ledger, "archive-clearing", &env->asset, 75000u, &env->vault_archive));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_lane(&env->ledger, "direct-south", &env->asset, &env->vault_north, &env->vault_south, &env->merchant, MD_LANE_STANDARD, 25u, 10u, &env->lane_direct));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_lane(&env->ledger, "priority-south", &env->asset, &env->vault_relay, &env->vault_south, &env->merchant, MD_LANE_PRIORITY, 40u, 20u, &env->lane_priority));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_add_lane(&env->ledger, "rebalance-zenith", &env->asset, &env->vault_archive, &env->vault_relay, &env->counterparty, MD_LANE_REBALANCE, 35u, 15u, &env->lane_rebalance));
    if (status != MD_OK) {
        return status;
    }
    status = require_status(md_ledger_deposit(&env->ledger, &env->payer, &env->asset, 1000000u));
    if (status != MD_OK) {
        return status;
    }
    return md_ledger_conservation_ok(&env->ledger, &env->asset) ? MD_OK : MD_ERR_STATE;
}

static MdSettlementPlan base_plan(const ScenarioEnv *env, const MdDigest *lane, MdAmount gross, MdAmount min_net, uint32_t max_fee_bps, uint64_t settle_after) {
    MdSettlementPlan plan;

    md_plan_init(&plan);
    plan.settlement_lane = *lane;
    plan.beneficiary = env->merchant;
    plan.operator_account = env->operator_account;
    plan.memo = md_digest_text("meridian-memo", "invoice:orion:net-30");
    plan.min_net_amount = min_net;
    plan.max_fee_bps = max_fee_bps;
    plan.quote_epoch = env->ledger.current_epoch;
    plan.settle_after_epoch = settle_after;
    (void)gross;
    return plan;
}

static MdSignedIntent sign_terms(ScenarioEnv *env, MdIntentTerms terms) {
    const MdAccount *payer = md_ledger_find_account_const(&env->ledger, &env->payer);

    if (payer == 0) {
        return md_signed_intent_create(0, terms);
    }
    return md_signed_intent_create(&payer->identity, terms);
}

static MdIntentTerms make_terms(ScenarioEnv *env, MdSettlementPlan plan, MdAmount gross, uint64_t nonce, uint64_t deadline, const char *memo) {
    MdIntentTerms terms;

    md_intent_terms_init(&terms);
    terms.network_id = env->ledger.network_id;
    terms.payer = env->payer;
    terms.asset_id = env->asset;
    terms.gross_amount = gross;
    terms.payer_nonce = nonce;
    terms.deadline_epoch = deadline;
    snprintf(terms.memo, sizeof(terms.memo), "%s", memo == 0 ? "" : memo);
    terms.plan = plan;
    terms.intent_id = md_intent_terms_digest(&terms);
    return terms;
}

static void print_digest_value(const MdDigest *digest) {
    printf("\"%s\"", digest == 0 ? "" : digest->hex);
}

static void print_balances(const ScenarioEnv *env) {
    printf("  \"balances\": {\n");
    printf("    \"payer\": %" PRIu64 ",\n", md_ledger_account_balance(&env->ledger, &env->payer, &env->asset));
    printf("    \"merchant\": %" PRIu64 ",\n", md_ledger_account_balance(&env->ledger, &env->merchant, &env->asset));
    printf("    \"operator\": %" PRIu64 ",\n", md_ledger_account_balance(&env->ledger, &env->operator_account, &env->asset));
    printf("    \"bridge\": %" PRIu64 ",\n", md_ledger_account_balance(&env->ledger, &env->bridge, &env->asset));
    printf("    \"counterparty\": %" PRIu64 ",\n", md_ledger_account_balance(&env->ledger, &env->counterparty, &env->asset));
    printf("    \"locked\": %" PRIu64 "\n", md_ledger_locked_total(&env->ledger, &env->asset));
    printf("  },\n");
}

static void print_vaults(const ScenarioEnv *env) {
    printf("  \"vaults\": {\n");
    printf("    \"north\": %" PRIu64 ",\n", md_ledger_vault_balance(&env->ledger, &env->vault_north));
    printf("    \"relay\": %" PRIu64 ",\n", md_ledger_vault_balance(&env->ledger, &env->vault_relay));
    printf("    \"south\": %" PRIu64 ",\n", md_ledger_vault_balance(&env->ledger, &env->vault_south));
    printf("    \"archive\": %" PRIu64 "\n", md_ledger_vault_balance(&env->ledger, &env->vault_archive));
    printf("  },\n");
}

static void print_lane_economics_object(const MdLaneEconomics *economics) {
    MdDigest digest = md_lane_economics_digest(economics);

    printf("{\n");
    printf("      \"lane\": ");
    print_digest_value(&economics->lane_id);
    printf(",\n");
    printf("      \"terminal\": ");
    print_digest_value(&economics->terminal_account);
    printf(",\n");
    printf("      \"gross\": %" PRIu64 ",\n", economics->gross_amount);
    printf("      \"net\": %" PRIu64 ",\n", economics->net_amount);
    printf("      \"operator_fee\": %" PRIu64 ",\n", economics->operator_fee);
    printf("      \"reserve_fee\": %" PRIu64 ",\n", economics->reserve_fee);
    printf("      \"fee_bps_total\": %u,\n", economics->fee_bps_total);
    printf("      \"digest\": ");
    print_digest_value(&digest);
    printf("\n");
    printf("    }");
}

static void print_vault_exposure_object(const MdVaultExposure *exposure) {
    MdDigest digest = md_vault_exposure_digest(exposure);

    printf("{\n");
    printf("      \"vault\": ");
    print_digest_value(&exposure->vault_id);
    printf(",\n");
    printf("      \"balance\": %" PRIu64 ",\n", exposure->balance);
    printf("      \"pending_in\": %" PRIu64 ",\n", exposure->pending_in);
    printf("      \"pending_out\": %" PRIu64 ",\n", exposure->pending_out);
    printf("      \"available\": %" PRIu64 ",\n", exposure->available);
    printf("      \"inbound_lanes\": %zu,\n", exposure->inbound_lanes);
    printf("      \"outbound_lanes\": %zu,\n", exposure->outbound_lanes);
    printf("      \"digest\": ");
    print_digest_value(&digest);
    printf("\n");
    printf("    }");
}

static void print_receipt(const MdReceipt *receipt) {
    if (receipt == 0) {
        printf("  \"receipt\": null,\n");
        return;
    }
    printf("  \"receipt\": {\n");
    printf("    \"id\": ");
    print_digest_value(&receipt->receipt_id);
    printf(",\n");
    printf("    \"lane\": ");
    print_digest_value(&receipt->effective_lane);
    printf(",\n");
    printf("    \"beneficiary\": ");
    print_digest_value(&receipt->effective_beneficiary);
    printf(",\n");
    printf("    \"route_digest\": ");
    print_digest_value(&receipt->route_digest);
    printf(",\n");
    printf("    \"gross\": %" PRIu64 ",\n", receipt->gross_amount);
    printf("    \"net\": %" PRIu64 ",\n", receipt->net_amount);
    printf("    \"operator_fee\": %" PRIu64 ",\n", receipt->operator_fee);
    printf("    \"reserve_fee\": %" PRIu64 ",\n", receipt->reserve_fee);
    printf("    \"epoch\": %" PRIu64 "\n", receipt->settlement_epoch);
    printf("  },\n");
}

static void print_route(const MdCompactedPlan *compacted) {
    if (compacted == 0) {
        printf("  \"route\": null,\n");
        return;
    }
    printf("  \"route\": {\n");
    printf("    \"before\": ");
    print_digest_value(&compacted->route_digest_before);
    printf(",\n");
    printf("    \"after\": ");
    print_digest_value(&compacted->route_digest_after);
    printf(",\n");
    printf("    \"effective_lane\": ");
    print_digest_value(&compacted->effective_lane);
    printf(",\n");
    printf("    \"removed_hops\": %zu,\n", compacted->removed_hops);
    printf("    \"hop_count\": %zu\n", compacted->plan.hop_count);
    printf("  },\n");
}

static void print_footer(const ScenarioEnv *env, const char *scenario) {
    MdDigest state = md_ledger_state_digest(&env->ledger);
    int asset_idx = md_ledger_asset_index(&env->ledger, &env->asset);

    printf("  \"scenario\": \"%s\",\n", scenario);
    printf("  \"network_id\": %u,\n", env->ledger.network_id);
    printf("  \"epoch\": %" PRIu64 ",\n", env->ledger.current_epoch);
    printf("  \"asset\": ");
    print_digest_value(&env->asset);
    printf(",\n");
    printf("  \"total_supply\": %" PRIu64 ",\n", asset_idx >= 0 ? env->ledger.issued_supply[asset_idx] : 0u);
    printf("  \"observed_supply\": %" PRIu64 ",\n", md_ledger_total_observed(&env->ledger, &env->asset));
    printf("  \"conservation_ok\": %s,\n", md_ledger_conservation_ok(&env->ledger, &env->asset) ? "true" : "false");
    printf("  \"state_digest\": ");
    print_digest_value(&state);
    printf(",\n");
    printf("  \"journal\": {\n");
    printf("    \"count\": %zu,\n", env->ledger.journal_count);
    if (env->ledger.journal_count == 0u) {
        printf("    \"last_event\": \"\"\n");
    } else {
        printf("    \"last_event\": \"%s\"\n", env->ledger.journal[env->ledger.journal_count - 1u].event);
    }
    printf("  }\n");
}

static void print_report(const ScenarioEnv *env, const char *scenario, const MdDigest *intent_id, const MdDigest *reservation_id, const MdReceipt *receipt, const MdCompactedPlan *compacted) {
    printf("{\n");
    if (intent_id != 0) {
        printf("  \"intent\": ");
        print_digest_value(intent_id);
        printf(",\n");
    }
    if (reservation_id != 0) {
        printf("  \"reservation\": ");
        print_digest_value(reservation_id);
        printf(",\n");
    }
    print_receipt(receipt);
    print_route(compacted);
    print_balances(env);
    print_vaults(env);
    print_footer(env, scenario);
    printf("}\n");
}

static MdStatus scenario_direct(void) {
    ScenarioEnv env;
    MdSettlementPlan plan;
    MdIntentTerms terms;
    MdSignedIntent signed_intent;
    MdDigest reservation_id;
    MdReceipt receipt;
    MdCompactedPlan compacted;
    const MdReservation *reservation;
    MdStatus status;

    status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    plan = base_plan(&env, &env.lane_direct, 100000u, 99000u, 100u, 2u);
    status = md_plan_add_hop(&plan, route_hop(&env.lane_direct, &env.vault_north, &env.vault_south, &env.bridge, 100000u, 10000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    terms = make_terms(&env, plan, 100000u, 0u, 8u, "direct-invoice");
    signed_intent = sign_terms(&env, terms);
    status = md_ledger_reserve_intent(&env.ledger, &signed_intent, &reservation_id);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 4u, &receipt);
    if (status != MD_OK) {
        return status;
    }
    reservation = md_ledger_find_reservation_const(&env.ledger, &reservation_id);
    if (reservation == 0) {
        return MD_ERR_INTERNAL;
    }
    status = md_plan_compact(&reservation->plan, &compacted);
    if (status != MD_OK) {
        return status;
    }
    print_report(&env, "direct", &terms.intent_id, &reservation_id, &receipt, &compacted);
    return MD_OK;
}

static MdStatus scenario_routed(void) {
    ScenarioEnv env;
    MdSettlementPlan plan;
    MdIntentTerms terms;
    MdSignedIntent signed_intent;
    MdDigest reservation_id;
    MdReceipt receipt;
    MdCompactedPlan compacted;
    const MdReservation *reservation;
    MdStatus status;

    status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    plan = base_plan(&env, &env.lane_priority, 150000u, 148000u, 100u, 2u);
    status = md_plan_add_hop(&plan, route_hop(&env.lane_direct, &env.vault_north, &env.vault_relay, &env.bridge, 150000u, 6000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    status = md_plan_add_hop(&plan, route_hop(&env.lane_priority, &env.vault_relay, &env.vault_south, &env.bridge, 150000u, 4000u, 2u));
    if (status != MD_OK) {
        return status;
    }
    terms = make_terms(&env, plan, 150000u, 0u, 9u, "routed-invoice");
    signed_intent = sign_terms(&env, terms);
    status = md_ledger_reserve_intent(&env.ledger, &signed_intent, &reservation_id);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 5u, &receipt);
    if (status != MD_OK) {
        return status;
    }
    reservation = md_ledger_find_reservation_const(&env.ledger, &reservation_id);
    if (reservation == 0) {
        return MD_ERR_INTERNAL;
    }
    status = md_plan_compact(&reservation->plan, &compacted);
    if (status != MD_OK) {
        return status;
    }
    print_report(&env, "routed", &terms.intent_id, &reservation_id, &receipt, &compacted);
    return MD_OK;
}

static MdStatus scenario_epoch_batch(void) {
    ScenarioEnv env;
    MdSettlementPlan first_plan;
    MdSettlementPlan second_plan;
    MdIntentTerms first_terms;
    MdIntentTerms second_terms;
    MdSignedIntent first_signed;
    MdSignedIntent second_signed;
    MdDigest first_reservation;
    MdDigest second_reservation;
    MdReceipt first_receipt;
    MdReceipt second_receipt;
    MdCompactedPlan compacted;
    const MdReservation *reservation;
    MdStatus status;

    status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    first_plan = base_plan(&env, &env.lane_direct, 80000u, 79000u, 100u, 2u);
    status = md_plan_add_hop(&first_plan, route_hop(&env.lane_direct, &env.vault_north, &env.vault_south, &env.bridge, 80000u, 10000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    first_terms = make_terms(&env, first_plan, 80000u, 0u, 9u, "batch-1");
    first_signed = sign_terms(&env, first_terms);
    status = md_ledger_reserve_intent(&env.ledger, &first_signed, &first_reservation);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_settle_reservation(&env.ledger, &first_reservation, 3u, &first_receipt);
    if (status != MD_OK) {
        return status;
    }

    second_plan = base_plan(&env, &env.lane_priority, 60000u, 59000u, 100u, 4u);
    status = md_plan_add_hop(&second_plan, route_hop(&env.lane_priority, &env.vault_relay, &env.vault_south, &env.bridge, 60000u, 10000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    second_terms = make_terms(&env, second_plan, 60000u, 1u, 10u, "batch-2");
    second_signed = sign_terms(&env, second_terms);
    status = md_ledger_reserve_intent(&env.ledger, &second_signed, &second_reservation);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_settle_reservation(&env.ledger, &second_reservation, 6u, &second_receipt);
    if (status != MD_OK) {
        return status;
    }
    reservation = md_ledger_find_reservation_const(&env.ledger, &second_reservation);
    if (reservation == 0) {
        return MD_ERR_INTERNAL;
    }
    status = md_plan_compact(&reservation->plan, &compacted);
    if (status != MD_OK) {
        return status;
    }
    printf("{\n");
    printf("  \"first_receipt\": ");
    print_digest_value(&first_receipt.receipt_id);
    printf(",\n");
    printf("  \"second_receipt\": ");
    print_digest_value(&second_receipt.receipt_id);
    printf(",\n");
    print_receipt(&second_receipt);
    print_route(&compacted);
    print_balances(&env);
    print_vaults(&env);
    print_footer(&env, "epoch-batch");
    printf("}\n");
    return MD_OK;
}

static MdStatus scenario_recompact(void) {
    ScenarioEnv env;
    MdSettlementPlan plan;
    MdIntentTerms terms;
    MdSignedIntent signed_intent;
    MdDigest reservation_id;
    MdReceipt receipt;
    MdCompactedPlan compacted;
    const MdReservation *reservation;
    MdStatus status;

    status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    plan = base_plan(&env, &env.lane_priority, 125000u, 123000u, 100u, 2u);
    status = md_plan_add_hop(&plan, route_hop(&env.lane_priority, &env.vault_north, &env.vault_relay, &env.bridge, 125000u, 3333u, 1u));
    if (status != MD_OK) {
        return status;
    }
    status = md_plan_add_hop(&plan, route_hop(&env.lane_priority, &env.vault_relay, &env.vault_south, &env.bridge, 125000u, 3333u, 2u));
    if (status != MD_OK) {
        return status;
    }
    status = md_plan_add_hop(&plan, route_hop(&env.lane_priority, &env.vault_south, &env.vault_archive, &env.bridge, 125000u, 3334u, 3u));
    if (status != MD_OK) {
        return status;
    }
    terms = make_terms(&env, plan, 125000u, 0u, 9u, "recompact-invoice");
    signed_intent = sign_terms(&env, terms);
    status = md_ledger_reserve_intent(&env.ledger, &signed_intent, &reservation_id);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 5u, &receipt);
    if (status != MD_OK) {
        return status;
    }
    reservation = md_ledger_find_reservation_const(&env.ledger, &reservation_id);
    if (reservation == 0) {
        return MD_ERR_INTERNAL;
    }
    status = md_plan_compact(&reservation->plan, &compacted);
    if (status != MD_OK) {
        return status;
    }
    print_report(&env, "recompact", &terms.intent_id, &reservation_id, &receipt, &compacted);
    return MD_OK;
}

static MdStatus scenario_rejections(void) {
    ScenarioEnv env;
    MdSettlementPlan plan;
    MdIntentTerms expired_terms;
    MdIntentTerms valid_terms;
    MdSignedIntent expired_signed;
    MdSignedIntent valid_signed;
    MdDigest reservation_id;
    MdReceipt receipt;
    MdStatus expired_status;
    MdStatus reserve_status;
    MdStatus duplicate_status;
    MdStatus early_settle_status;
    MdStatus settle_status;

    MdStatus status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    plan = base_plan(&env, &env.lane_direct, 50000u, 49000u, 100u, 3u);
    status = md_plan_add_hop(&plan, route_hop(&env.lane_direct, &env.vault_north, &env.vault_south, &env.bridge, 50000u, 10000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    expired_terms = make_terms(&env, plan, 50000u, 0u, 0u, "expired");
    expired_signed = sign_terms(&env, expired_terms);
    expired_status = md_ledger_reserve_intent(&env.ledger, &expired_signed, 0);

    valid_terms = make_terms(&env, plan, 50000u, 0u, 9u, "valid");
    valid_signed = sign_terms(&env, valid_terms);
    reserve_status = md_ledger_reserve_intent(&env.ledger, &valid_signed, &reservation_id);
    duplicate_status = md_ledger_reserve_intent(&env.ledger, &valid_signed, 0);
    early_settle_status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 2u, 0);
    settle_status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 4u, &receipt);
    if (reserve_status != MD_OK || settle_status != MD_OK) {
        return reserve_status != MD_OK ? reserve_status : settle_status;
    }
    printf("{\n");
    printf("  \"expired_status\": \"%s\",\n", md_status_name(expired_status));
    printf("  \"reserve_status\": \"%s\",\n", md_status_name(reserve_status));
    printf("  \"duplicate_status\": \"%s\",\n", md_status_name(duplicate_status));
    printf("  \"early_settle_status\": \"%s\",\n", md_status_name(early_settle_status));
    printf("  \"settle_status\": \"%s\",\n", md_status_name(settle_status));
    printf("  \"receipt\": ");
    print_digest_value(&receipt.receipt_id);
    printf(",\n");
    print_balances(&env);
    print_vaults(&env);
    print_footer(&env, "rejections");
    printf("}\n");
    return MD_OK;
}

static MdStatus scenario_operations(void) {
    ScenarioEnv env;
    MdSettlementPlan plan;
    MdIntentTerms terms;
    MdSignedIntent signed_intent;
    MdDigest reservation_id;
    MdLaneEconomics direct_quote;
    MdLaneEconomics priority_quote;
    MdVaultExposure south_before;
    MdVaultExposure north_reserved;
    MdVaultExposure south_reserved;
    MdVaultExposure north_cancelled;
    MdVaultExposure south_cancelled;
    MdStatus reserve_status;
    MdStatus cancel_status;
    MdStatus settle_cancelled_status;
    const MdReservation *reservation;
    MdStatus status;

    status = setup_env(&env);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_lane_economics(&env.ledger, &env.lane_direct, 120000u, &direct_quote);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_lane_economics(&env.ledger, &env.lane_priority, 120000u, &priority_quote);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_vault_exposure(&env.ledger, &env.vault_south, &south_before);
    if (status != MD_OK) {
        return status;
    }

    plan = base_plan(&env, &env.lane_direct, 70000u, 69000u, 100u, 7u);
    status = md_plan_add_hop(&plan, route_hop(&env.lane_direct, &env.vault_north, &env.vault_south, &env.bridge, 70000u, 10000u, 1u));
    if (status != MD_OK) {
        return status;
    }
    terms = make_terms(&env, plan, 70000u, 0u, 12u, "cancel-window");
    signed_intent = sign_terms(&env, terms);
    reserve_status = md_ledger_reserve_intent(&env.ledger, &signed_intent, &reservation_id);
    if (reserve_status != MD_OK) {
        return reserve_status;
    }
    status = md_ledger_vault_exposure(&env.ledger, &env.vault_north, &north_reserved);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_vault_exposure(&env.ledger, &env.vault_south, &south_reserved);
    if (status != MD_OK) {
        return status;
    }
    cancel_status = md_ledger_cancel_reservation(&env.ledger, &reservation_id, 3u);
    if (cancel_status != MD_OK) {
        return cancel_status;
    }
    settle_cancelled_status = md_ledger_settle_reservation(&env.ledger, &reservation_id, 7u, 0);
    status = md_ledger_vault_exposure(&env.ledger, &env.vault_north, &north_cancelled);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_vault_exposure(&env.ledger, &env.vault_south, &south_cancelled);
    if (status != MD_OK) {
        return status;
    }
    reservation = md_ledger_find_reservation_const(&env.ledger, &reservation_id);
    if (reservation == 0) {
        return MD_ERR_INTERNAL;
    }

    printf("{\n");
    printf("  \"lane_economics\": {\n");
    printf("    \"direct\": ");
    print_lane_economics_object(&direct_quote);
    printf(",\n");
    printf("    \"priority\": ");
    print_lane_economics_object(&priority_quote);
    printf("\n");
    printf("  },\n");
    printf("  \"exposure_before\": {\n");
    printf("    \"south\": ");
    print_vault_exposure_object(&south_before);
    printf("\n");
    printf("  },\n");
    printf("  \"exposure_reserved\": {\n");
    printf("    \"north\": ");
    print_vault_exposure_object(&north_reserved);
    printf(",\n");
    printf("    \"south\": ");
    print_vault_exposure_object(&south_reserved);
    printf("\n");
    printf("  },\n");
    printf("  \"exposure_cancelled\": {\n");
    printf("    \"north\": ");
    print_vault_exposure_object(&north_cancelled);
    printf(",\n");
    printf("    \"south\": ");
    print_vault_exposure_object(&south_cancelled);
    printf("\n");
    printf("  },\n");
    printf("  \"reservation\": ");
    print_digest_value(&reservation_id);
    printf(",\n");
    printf("  \"reservation_state\": %u,\n", (uint32_t)reservation->state);
    printf("  \"reserve_status\": \"%s\",\n", md_status_name(reserve_status));
    printf("  \"cancel_status\": \"%s\",\n", md_status_name(cancel_status));
    printf("  \"settle_cancelled_status\": \"%s\",\n", md_status_name(settle_cancelled_status));
    print_balances(&env);
    print_vaults(&env);
    print_footer(&env, "operations");
    printf("}\n");
    return MD_OK;
}

MdStatus md_runtime_run(const char *scenario) {
    if (scenario == 0 || scenario[0] == '\0' || strcmp(scenario, "routed") == 0) {
        return scenario_routed();
    }
    if (strcmp(scenario, "direct") == 0) {
        return scenario_direct();
    }
    if (strcmp(scenario, "epoch-batch") == 0) {
        return scenario_epoch_batch();
    }
    if (strcmp(scenario, "recompact") == 0) {
        return scenario_recompact();
    }
    if (strcmp(scenario, "rejections") == 0) {
        return scenario_rejections();
    }
    if (strcmp(scenario, "operations") == 0) {
        return scenario_operations();
    }
    fprintf(stderr, "unknown scenario: %s\n", scenario);
    return MD_ERR_ARGUMENT;
}
