#ifndef MERIDIAN_LEDGER_H
#define MERIDIAN_LEDGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meridian/amount.h"
#include "meridian/crypto.h"
#include "meridian/digest.h"
#include "meridian/route.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_MAX_ASSETS 4u
#define MD_MAX_ACCOUNTS 16u
#define MD_MAX_VAULTS 16u
#define MD_MAX_LANES 16u
#define MD_MAX_RESERVATIONS 32u
#define MD_MAX_RECEIPTS 32u
#define MD_MAX_JOURNAL 96u
#define MD_SYMBOL_LENGTH 15u
#define MD_ROLE_LENGTH 31u
#define MD_EVENT_LENGTH 95u

typedef struct MdAsset {
    MdDigest id;
    char symbol[MD_SYMBOL_LENGTH + 1u];
    uint8_t decimals;
} MdAsset;

typedef struct MdAccount {
    MdDigest id;
    MdIdentity identity;
    char role[MD_ROLE_LENGTH + 1u];
    MdAmount balances[MD_MAX_ASSETS];
    uint64_t intent_nonce;
    uint64_t receipt_nonce;
    bool active;
} MdAccount;

typedef struct MdVault {
    MdDigest id;
    char label[MD_ROLE_LENGTH + 1u];
    MdDigest asset_id;
    MdAmount balance;
    MdAmount pending_out;
    MdAmount pending_in;
    bool active;
} MdVault;

typedef struct MdSettlementLane {
    MdDigest id;
    char label[MD_ROLE_LENGTH + 1u];
    MdDigest asset_id;
    MdDigest source_vault;
    MdDigest sink_vault;
    MdDigest terminal_account;
    MdLaneClass lane_class;
    uint32_t operator_fee_bps;
    uint32_t reserve_bps;
    bool active;
} MdSettlementLane;

typedef struct MdLaneEconomics {
    MdDigest lane_id;
    MdDigest terminal_account;
    MdDigest source_vault;
    MdDigest sink_vault;
    MdAmount gross_amount;
    MdAmount operator_fee;
    MdAmount reserve_fee;
    MdAmount net_amount;
    uint32_t fee_bps_total;
} MdLaneEconomics;

typedef struct MdVaultExposure {
    MdDigest vault_id;
    MdDigest asset_id;
    MdAmount balance;
    MdAmount pending_in;
    MdAmount pending_out;
    MdAmount available;
    size_t inbound_lanes;
    size_t outbound_lanes;
} MdVaultExposure;

typedef enum MdReservationState {
    MD_RESERVATION_EMPTY = 0,
    MD_RESERVATION_RESERVED = 1,
    MD_RESERVATION_SETTLED = 2,
    MD_RESERVATION_CANCELLED = 3
} MdReservationState;

typedef struct MdReservation {
    MdDigest reservation_id;
    MdDigest intent_id;
    MdDigest payer;
    MdDigest asset_id;
    MdAmount gross_amount;
    MdSettlementPlan plan;
    MdDigest route_digest;
    uint64_t deadline_epoch;
    uint64_t reserved_epoch;
    MdReservationState state;
} MdReservation;

typedef struct MdReceipt {
    MdDigest receipt_id;
    MdDigest reservation_id;
    MdDigest effective_lane;
    MdDigest effective_beneficiary;
    MdDigest route_digest;
    MdAmount gross_amount;
    MdAmount net_amount;
    MdAmount operator_fee;
    MdAmount reserve_fee;
    uint64_t settlement_epoch;
    bool settled;
} MdReceipt;

typedef struct MdJournalEntry {
    uint64_t epoch;
    char event[MD_EVENT_LENGTH + 1u];
    MdDigest subject;
    MdAmount amount;
} MdJournalEntry;

typedef struct MdLedger {
    uint32_t network_id;
    uint64_t current_epoch;
    MdAsset assets[MD_MAX_ASSETS];
    size_t asset_count;
    MdAccount accounts[MD_MAX_ACCOUNTS];
    size_t account_count;
    MdVault vaults[MD_MAX_VAULTS];
    size_t vault_count;
    MdSettlementLane lanes[MD_MAX_LANES];
    size_t lane_count;
    MdReservation reservations[MD_MAX_RESERVATIONS];
    size_t reservation_count;
    MdReceipt receipts[MD_MAX_RECEIPTS];
    size_t receipt_count;
    MdJournalEntry journal[MD_MAX_JOURNAL];
    size_t journal_count;
    MdAmount issued_supply[MD_MAX_ASSETS];
} MdLedger;

void md_ledger_init(MdLedger *ledger, uint32_t network_id);
MdStatus md_ledger_add_asset(MdLedger *ledger, const char *symbol, uint8_t decimals, MdDigest *out_id);
MdStatus md_ledger_add_account(MdLedger *ledger, const char *label, const char *role, MdDigest *out_id);
MdStatus md_ledger_add_vault(MdLedger *ledger, const char *label, const MdDigest *asset_id, MdAmount initial_balance, MdDigest *out_id);
MdStatus md_ledger_add_lane(MdLedger *ledger, const char *label, const MdDigest *asset_id, const MdDigest *source_vault, const MdDigest *sink_vault, const MdDigest *terminal_account, MdLaneClass lane_class, uint32_t operator_fee_bps, uint32_t reserve_bps, MdDigest *out_id);
MdStatus md_ledger_deposit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount);
MdStatus md_ledger_debit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount);
MdStatus md_ledger_credit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount);
MdStatus md_ledger_credit_vault(MdLedger *ledger, const MdDigest *vault_id, MdAmount amount);
MdStatus md_ledger_debit_vault(MdLedger *ledger, const MdDigest *vault_id, MdAmount amount);
MdAccount *md_ledger_find_account(MdLedger *ledger, const MdDigest *account_id);
const MdAccount *md_ledger_find_account_const(const MdLedger *ledger, const MdDigest *account_id);
MdAsset *md_ledger_find_asset(MdLedger *ledger, const MdDigest *asset_id);
const MdAsset *md_ledger_find_asset_const(const MdLedger *ledger, const MdDigest *asset_id);
MdVault *md_ledger_find_vault(MdLedger *ledger, const MdDigest *vault_id);
const MdVault *md_ledger_find_vault_const(const MdLedger *ledger, const MdDigest *vault_id);
MdSettlementLane *md_ledger_find_lane(MdLedger *ledger, const MdDigest *lane_id);
const MdSettlementLane *md_ledger_find_lane_const(const MdLedger *ledger, const MdDigest *lane_id);
MdReservation *md_ledger_find_reservation(MdLedger *ledger, const MdDigest *reservation_id);
const MdReservation *md_ledger_find_reservation_const(const MdLedger *ledger, const MdDigest *reservation_id);
MdReceipt *md_ledger_find_receipt(MdLedger *ledger, const MdDigest *receipt_id);
MdStatus md_ledger_append_journal(MdLedger *ledger, uint64_t epoch, const char *event, const MdDigest *subject, MdAmount amount);
MdAmount md_ledger_account_balance(const MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id);
MdAmount md_ledger_vault_balance(const MdLedger *ledger, const MdDigest *vault_id);
MdAmount md_ledger_locked_total(const MdLedger *ledger, const MdDigest *asset_id);
MdAmount md_ledger_total_observed(const MdLedger *ledger, const MdDigest *asset_id);
bool md_ledger_conservation_ok(const MdLedger *ledger, const MdDigest *asset_id);
MdDigest md_ledger_state_digest(const MdLedger *ledger);
int md_ledger_asset_index(const MdLedger *ledger, const MdDigest *asset_id);
MdStatus md_ledger_lane_economics(const MdLedger *ledger, const MdDigest *lane_id, MdAmount gross_amount, MdLaneEconomics *out);
MdStatus md_ledger_vault_exposure(const MdLedger *ledger, const MdDigest *vault_id, MdVaultExposure *out);
MdDigest md_lane_economics_digest(const MdLaneEconomics *economics);
MdDigest md_vault_exposure_digest(const MdVaultExposure *exposure);

#ifdef __cplusplus
}
#endif

#endif
