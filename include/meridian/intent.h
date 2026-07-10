#ifndef MERIDIAN_INTENT_H
#define MERIDIAN_INTENT_H

#include <stddef.h>
#include <stdint.h>

#include "meridian/crypto.h"
#include "meridian/ledger.h"
#include "meridian/route.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_MEMO_LENGTH 63u

typedef struct MdIntentTerms {
    uint32_t network_id;
    MdDigest intent_id;
    MdDigest payer;
    MdDigest asset_id;
    MdAmount gross_amount;
    uint64_t payer_nonce;
    uint64_t deadline_epoch;
    char memo[MD_MEMO_LENGTH + 1u];
    MdSettlementPlan plan;
} MdIntentTerms;

typedef struct MdSignedIntent {
    MdIntentTerms terms;
    MdSignature signature;
} MdSignedIntent;

typedef struct MdSettlementQuote {
    MdDigest lane_id;
    MdDigest beneficiary;
    MdAmount gross_amount;
    MdAmount operator_fee;
    MdAmount reserve_fee;
    MdAmount net_amount;
} MdSettlementQuote;

void md_intent_terms_init(MdIntentTerms *terms);
MdStatus md_intent_authorization_payload(const MdIntentTerms *terms, char *out, size_t out_len);
MdDigest md_intent_terms_digest(const MdIntentTerms *terms);
MdSignedIntent md_signed_intent_create(const MdIdentity *payer, MdIntentTerms terms);
MdStatus md_signed_intent_verify(const MdLedger *ledger, const MdSignedIntent *signed_intent);
MdStatus md_ledger_reserve_intent(MdLedger *ledger, const MdSignedIntent *signed_intent, MdDigest *out_reservation);
MdStatus md_quote_settlement(const MdLedger *ledger, const MdReservation *reservation, const MdCompactedPlan *compacted, MdSettlementQuote *quote);
MdStatus md_ledger_settle_reservation(MdLedger *ledger, const MdDigest *reservation_id, uint64_t settlement_epoch, MdReceipt *out_receipt);
MdStatus md_ledger_cancel_reservation(MdLedger *ledger, const MdDigest *reservation_id, uint64_t cancel_epoch);
MdDigest md_receipt_digest(const MdReceipt *receipt);

#ifdef __cplusplus
}
#endif

#endif
