#include "meridian/intent.h"

#include <stdio.h>
#include <string.h>

#define MD_INTENT_DOMAIN "meridian-intent-v1"
#define MD_RECEIPT_DOMAIN "meridian-receipt-v1"

void md_intent_terms_init(MdIntentTerms *terms) {
    if (terms == 0) {
        return;
    }
    memset(terms, 0, sizeof(*terms));
    md_digest_to_hex(&terms->intent_id);
    md_digest_to_hex(&terms->payer);
    md_digest_to_hex(&terms->asset_id);
    md_plan_init(&terms->plan);
}

MdStatus md_intent_authorization_payload(const MdIntentTerms *terms, char *out, size_t out_len) {
    MdWriter writer;

    if (terms == 0 || out == 0 || out_len == 0u) {
        return MD_ERR_ARGUMENT;
    }
    md_writer_init(&writer, out, out_len);
    md_writer_put(&writer, "intent{");
    md_writer_u32(&writer, "network", terms->network_id);
    md_writer_digest(&writer, "intent", &terms->intent_id);
    md_writer_digest(&writer, "payer", &terms->payer);
    md_writer_digest(&writer, "asset", &terms->asset_id);
    md_writer_amount(&writer, "gross", terms->gross_amount);
    md_writer_u64(&writer, "nonce", terms->payer_nonce);
    md_writer_u64(&writer, "deadline", terms->deadline_epoch);
    md_writer_field(&writer, "memo", terms->memo);
    md_writer_put(&writer, "}");
    return md_writer_status(&writer);
}

MdDigest md_intent_terms_digest(const MdIntentTerms *terms) {
    char payload[1024];

    if (md_intent_authorization_payload(terms, payload, sizeof(payload)) != MD_OK) {
        return md_digest_text("meridian-intent-id", "invalid");
    }
    return md_digest_canonical("meridian-intent-id", payload);
}

MdSignedIntent md_signed_intent_create(const MdIdentity *payer, MdIntentTerms terms) {
    MdSignedIntent signed_intent;
    char payload[1024];

    memset(&signed_intent, 0, sizeof(signed_intent));
    signed_intent.terms = terms;
    if (md_intent_authorization_payload(&terms, payload, sizeof(payload)) == MD_OK) {
        signed_intent.signature = md_sign_payload(payer, MD_INTENT_DOMAIN, payload);
    } else {
        md_digest_to_hex(&signed_intent.signature.signer);
        md_digest_to_hex(&signed_intent.signature.authenticator);
    }
    return signed_intent;
}

MdStatus md_signed_intent_verify(const MdLedger *ledger, const MdSignedIntent *signed_intent) {
    const MdAccount *payer;
    char payload[1024];

    if (ledger == 0 || signed_intent == 0) {
        return MD_ERR_ARGUMENT;
    }
    payer = md_ledger_find_account_const(ledger, &signed_intent->terms.payer);
    if (payer == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_intent_authorization_payload(&signed_intent->terms, payload, sizeof(payload)) != MD_OK) {
        return MD_ERR_INTERNAL;
    }
    if (!md_verify_payload(&payer->identity, MD_INTENT_DOMAIN, payload, &signed_intent->signature)) {
        return MD_ERR_SIGNATURE;
    }
    return MD_OK;
}

static MdStatus md_validate_hop_catalog(const MdLedger *ledger, const MdIntentTerms *terms, const MdRouteHop *hop) {
    const MdSettlementLane *lane;
    const MdVault *from_vault;
    const MdVault *to_vault;

    if (ledger == 0 || terms == 0 || hop == 0) {
        return MD_ERR_ARGUMENT;
    }
    lane = md_ledger_find_lane_const(ledger, &hop->lane_id);
    from_vault = md_ledger_find_vault_const(ledger, &hop->from_vault);
    to_vault = md_ledger_find_vault_const(ledger, &hop->to_vault);
    if (lane == 0 || from_vault == 0 || to_vault == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (!md_digest_equal(&lane->asset_id, &terms->asset_id) ||
        !md_digest_equal(&from_vault->asset_id, &terms->asset_id) ||
        !md_digest_equal(&to_vault->asset_id, &terms->asset_id)) {
        return MD_ERR_POLICY;
    }
    if (hop->weight_bps > MD_BPS_DENOMINATOR || hop->amount_hint > terms->gross_amount) {
        return MD_ERR_POLICY;
    }
    return MD_OK;
}

static MdStatus md_validate_plan_catalog(const MdLedger *ledger, const MdIntentTerms *terms) {
    const MdSettlementLane *lane;
    MdAmount fee_bps_sum;

    if (ledger == 0 || terms == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (terms->plan.hop_count == 0u || terms->plan.hop_count > MD_MAX_ROUTE_HOPS) {
        return MD_ERR_POLICY;
    }
    lane = md_ledger_find_lane_const(ledger, &terms->plan.settlement_lane);
    if (lane == 0 || !lane->active) {
        return MD_ERR_NOT_FOUND;
    }
    if (!md_digest_equal(&lane->asset_id, &terms->asset_id)) {
        return MD_ERR_POLICY;
    }
    if (md_ledger_find_account_const(ledger, &terms->plan.beneficiary) == 0 ||
        md_ledger_find_account_const(ledger, &terms->plan.operator_account) == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_add((MdAmount)lane->operator_fee_bps, (MdAmount)lane->reserve_bps, &fee_bps_sum) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (fee_bps_sum > terms->plan.max_fee_bps) {
        return MD_ERR_POLICY;
    }
    for (size_t i = 0u; i < terms->plan.hop_count; i++) {
        MdStatus status = md_validate_hop_catalog(ledger, terms, &terms->plan.hops[i]);
        if (status != MD_OK) {
            return status;
        }
    }
    return MD_OK;
}

static MdStatus md_check_duplicate_intent(const MdLedger *ledger, const MdDigest *intent_id) {
    if (ledger == 0 || intent_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    for (size_t i = 0u; i < ledger->reservation_count; i++) {
        if (md_digest_equal(&ledger->reservations[i].intent_id, intent_id)) {
            return MD_ERR_DUPLICATE;
        }
    }
    return MD_OK;
}

static MdStatus md_apply_pending_amount(MdAmount current, MdAmount delta, int release, MdAmount *out) {
    if (release != 0) {
        return md_amount_sub(current, delta, out);
    }
    return md_amount_add(current, delta, out);
}

static MdStatus md_update_route_pending(MdLedger *ledger, const MdSettlementPlan *plan, int release) {
    if (ledger == 0 || plan == 0) {
        return MD_ERR_ARGUMENT;
    }
    for (size_t i = 0u; i < plan->hop_count; i++) {
        MdVault *from_vault = md_ledger_find_vault(ledger, &plan->hops[i].from_vault);
        MdVault *to_vault = md_ledger_find_vault(ledger, &plan->hops[i].to_vault);
        MdAmount amount = plan->hops[i].amount_hint;
        MdAmount updated_from;
        MdAmount updated_to;

        if (from_vault == 0 || to_vault == 0) {
            return MD_ERR_NOT_FOUND;
        }
        if (md_apply_pending_amount(from_vault->pending_out, amount, release, &updated_from) != MD_OK ||
            md_apply_pending_amount(to_vault->pending_in, amount, release, &updated_to) != MD_OK) {
            return MD_ERR_STATE;
        }
        from_vault->pending_out = updated_from;
        to_vault->pending_in = updated_to;
    }
    return MD_OK;
}

MdStatus md_ledger_reserve_intent(MdLedger *ledger, const MdSignedIntent *signed_intent, MdDigest *out_reservation) {
    MdReservation *reservation;
    MdAccount *payer;
    MdStatus status;
    MdDigest route_digest;
    MdDigest reservation_id;

    if (ledger == 0 || signed_intent == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->reservation_count >= MD_MAX_RESERVATIONS) {
        return MD_ERR_CAPACITY;
    }
    status = md_signed_intent_verify(ledger, signed_intent);
    if (status != MD_OK) {
        return status;
    }
    if (signed_intent->terms.network_id != ledger->network_id) {
        return MD_ERR_POLICY;
    }
    if (signed_intent->terms.deadline_epoch < ledger->current_epoch) {
        return MD_ERR_EXPIRED;
    }
    payer = md_ledger_find_account(ledger, &signed_intent->terms.payer);
    if (payer == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (payer->intent_nonce != signed_intent->terms.payer_nonce) {
        return MD_ERR_DUPLICATE;
    }
    status = md_validate_plan_catalog(ledger, &signed_intent->terms);
    if (status != MD_OK) {
        return status;
    }
    status = md_check_duplicate_intent(ledger, &signed_intent->terms.intent_id);
    if (status != MD_OK) {
        return status;
    }
    if (md_ledger_account_balance(ledger, &signed_intent->terms.payer, &signed_intent->terms.asset_id) < signed_intent->terms.gross_amount) {
        return MD_ERR_BALANCE;
    }
    status = md_update_route_pending(ledger, &signed_intent->terms.plan, 0);
    if (status != MD_OK) {
        return status;
    }
    route_digest = md_plan_digest(&signed_intent->terms.plan);
    reservation_id = md_digest_join("meridian-reservation", &signed_intent->terms.intent_id, &route_digest);
    if (md_ledger_find_reservation(ledger, &reservation_id) != 0) {
        (void)md_update_route_pending(ledger, &signed_intent->terms.plan, 1);
        return MD_ERR_DUPLICATE;
    }
    status = md_ledger_debit(ledger, &signed_intent->terms.payer, &signed_intent->terms.asset_id, signed_intent->terms.gross_amount);
    if (status != MD_OK) {
        (void)md_update_route_pending(ledger, &signed_intent->terms.plan, 1);
        return status;
    }
    payer->intent_nonce++;
    reservation = &ledger->reservations[ledger->reservation_count];
    memset(reservation, 0, sizeof(*reservation));
    reservation->reservation_id = reservation_id;
    reservation->intent_id = signed_intent->terms.intent_id;
    reservation->payer = signed_intent->terms.payer;
    reservation->asset_id = signed_intent->terms.asset_id;
    reservation->gross_amount = signed_intent->terms.gross_amount;
    reservation->plan = signed_intent->terms.plan;
    reservation->route_digest = route_digest;
    reservation->deadline_epoch = signed_intent->terms.deadline_epoch;
    reservation->reserved_epoch = ledger->current_epoch;
    reservation->state = MD_RESERVATION_RESERVED;
    ledger->reservation_count++;
    status = md_ledger_append_journal(ledger, ledger->current_epoch, "reserve", &reservation_id, reservation->gross_amount);
    if (status != MD_OK) {
        return status;
    }
    if (!md_ledger_conservation_ok(ledger, &reservation->asset_id)) {
        return MD_ERR_STATE;
    }
    if (out_reservation != 0) {
        *out_reservation = reservation_id;
    }
    return MD_OK;
}

MdStatus md_quote_settlement(const MdLedger *ledger, const MdReservation *reservation, const MdCompactedPlan *compacted, MdSettlementQuote *quote) {
    const MdSettlementLane *lane;
    MdAmount fee_total;
    MdAmount allowed_fee;
    MdAmount after_fee;

    if (ledger == 0 || reservation == 0 || compacted == 0 || quote == 0) {
        return MD_ERR_ARGUMENT;
    }
    memset(quote, 0, sizeof(*quote));
    lane = md_ledger_find_lane_const(ledger, &compacted->effective_lane);
    if (lane == 0 || !lane->active) {
        return MD_ERR_NOT_FOUND;
    }
    if (!md_digest_equal(&lane->asset_id, &reservation->asset_id)) {
        return MD_ERR_POLICY;
    }
    quote->lane_id = lane->id;
    quote->beneficiary = lane->terminal_account;
    quote->gross_amount = reservation->gross_amount;
    if (md_amount_mul_bps(reservation->gross_amount, lane->operator_fee_bps, &quote->operator_fee) != MD_OK ||
        md_amount_mul_bps(reservation->gross_amount, lane->reserve_bps, &quote->reserve_fee) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (md_amount_add(quote->operator_fee, quote->reserve_fee, &fee_total) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (md_amount_mul_bps(reservation->gross_amount, reservation->plan.max_fee_bps, &allowed_fee) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (fee_total > allowed_fee) {
        return MD_ERR_POLICY;
    }
    if (md_amount_sub(reservation->gross_amount, fee_total, &after_fee) != MD_OK) {
        return MD_ERR_POLICY;
    }
    quote->net_amount = after_fee;
    if (quote->net_amount < reservation->plan.min_net_amount) {
        return MD_ERR_POLICY;
    }
    if (md_ledger_find_account_const(ledger, &quote->beneficiary) == 0 ||
        md_ledger_find_account_const(ledger, &reservation->plan.operator_account) == 0) {
        return MD_ERR_NOT_FOUND;
    }
    return MD_OK;
}

MdDigest md_receipt_digest(const MdReceipt *receipt) {
    char payload[2048];
    MdWriter writer;

    if (receipt == 0) {
        return md_digest_text(MD_RECEIPT_DOMAIN, "null-receipt");
    }
    md_writer_init(&writer, payload, sizeof(payload));
    md_writer_put(&writer, "receipt{");
    md_writer_digest(&writer, "reservation", &receipt->reservation_id);
    md_writer_digest(&writer, "lane", &receipt->effective_lane);
    md_writer_digest(&writer, "beneficiary", &receipt->effective_beneficiary);
    md_writer_digest(&writer, "route", &receipt->route_digest);
    md_writer_amount(&writer, "gross", receipt->gross_amount);
    md_writer_amount(&writer, "net", receipt->net_amount);
    md_writer_amount(&writer, "operator_fee", receipt->operator_fee);
    md_writer_amount(&writer, "reserve_fee", receipt->reserve_fee);
    md_writer_u64(&writer, "epoch", receipt->settlement_epoch);
    md_writer_put(&writer, "}");
    if (md_writer_status(&writer) != MD_OK) {
        return md_digest_text(MD_RECEIPT_DOMAIN, "invalid-receipt");
    }
    return md_digest_canonical(MD_RECEIPT_DOMAIN, payload);
}

MdStatus md_ledger_settle_reservation(MdLedger *ledger, const MdDigest *reservation_id, uint64_t settlement_epoch, MdReceipt *out_receipt) {
    MdReservation *reservation;
    MdCompactedPlan compacted;
    MdSettlementQuote quote;
    MdReceipt receipt;
    const MdSettlementLane *lane;
    MdStatus status;

    if (ledger == 0 || reservation_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->receipt_count >= MD_MAX_RECEIPTS) {
        return MD_ERR_CAPACITY;
    }
    reservation = md_ledger_find_reservation(ledger, reservation_id);
    if (reservation == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (reservation->state != MD_RESERVATION_RESERVED) {
        return MD_ERR_STATE;
    }
    if (settlement_epoch < reservation->plan.settle_after_epoch) {
        return MD_ERR_POLICY;
    }
    if (settlement_epoch > reservation->deadline_epoch) {
        return MD_ERR_EXPIRED;
    }
    status = md_plan_compact(&reservation->plan, &compacted);
    if (status != MD_OK) {
        return status;
    }
    status = md_quote_settlement(ledger, reservation, &compacted, &quote);
    if (status != MD_OK) {
        return status;
    }
    status = md_update_route_pending(ledger, &reservation->plan, 1);
    if (status != MD_OK) {
        return status;
    }
    lane = md_ledger_find_lane_const(ledger, &quote.lane_id);
    if (lane == 0) {
        return MD_ERR_NOT_FOUND;
    }
    status = md_ledger_credit(ledger, &quote.beneficiary, &reservation->asset_id, quote.net_amount);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_credit(ledger, &reservation->plan.operator_account, &reservation->asset_id, quote.operator_fee);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_credit_vault(ledger, &lane->sink_vault, quote.reserve_fee);
    if (status != MD_OK) {
        return status;
    }
    reservation->state = MD_RESERVATION_SETTLED;
    ledger->current_epoch = settlement_epoch;
    memset(&receipt, 0, sizeof(receipt));
    receipt.reservation_id = reservation->reservation_id;
    receipt.effective_lane = quote.lane_id;
    receipt.effective_beneficiary = quote.beneficiary;
    receipt.route_digest = compacted.route_digest_after;
    receipt.gross_amount = quote.gross_amount;
    receipt.net_amount = quote.net_amount;
    receipt.operator_fee = quote.operator_fee;
    receipt.reserve_fee = quote.reserve_fee;
    receipt.settlement_epoch = settlement_epoch;
    receipt.settled = true;
    receipt.receipt_id = md_receipt_digest(&receipt);
    ledger->receipts[ledger->receipt_count] = receipt;
    ledger->receipt_count++;
    status = md_ledger_append_journal(ledger, settlement_epoch, "settle", &receipt.receipt_id, receipt.gross_amount);
    if (status != MD_OK) {
        return status;
    }
    if (!md_ledger_conservation_ok(ledger, &reservation->asset_id)) {
        return MD_ERR_STATE;
    }
    if (out_receipt != 0) {
        *out_receipt = receipt;
    }
    return MD_OK;
}

MdStatus md_ledger_cancel_reservation(MdLedger *ledger, const MdDigest *reservation_id, uint64_t cancel_epoch) {
    MdReservation *reservation;
    MdStatus status;

    if (ledger == 0 || reservation_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    reservation = md_ledger_find_reservation(ledger, reservation_id);
    if (reservation == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (reservation->state != MD_RESERVATION_RESERVED) {
        return MD_ERR_STATE;
    }
    if (cancel_epoch < reservation->reserved_epoch) {
        return MD_ERR_POLICY;
    }
    if (cancel_epoch >= reservation->plan.settle_after_epoch && cancel_epoch <= reservation->deadline_epoch) {
        return MD_ERR_POLICY;
    }
    status = md_update_route_pending(ledger, &reservation->plan, 1);
    if (status != MD_OK) {
        return status;
    }
    status = md_ledger_credit(ledger, &reservation->payer, &reservation->asset_id, reservation->gross_amount);
    if (status != MD_OK) {
        return status;
    }
    reservation->state = MD_RESERVATION_CANCELLED;
    ledger->current_epoch = cancel_epoch;
    status = md_ledger_append_journal(ledger, cancel_epoch, "cancel", &reservation->reservation_id, reservation->gross_amount);
    if (status != MD_OK) {
        return status;
    }
    return md_ledger_conservation_ok(ledger, &reservation->asset_id) ? MD_OK : MD_ERR_STATE;
}
