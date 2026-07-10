#include "meridian/ledger.h"

#include <stdio.h>
#include <string.h>

static MdDigest md_named_digest2(const char *domain, const char *label, const MdDigest *id) {
    MdHash hash;

    md_hash_init(&hash, domain);
    md_hash_update_cstr(&hash, label);
    md_hash_update(&hash, "|", 1u);
    if (id != 0) {
        md_hash_update(&hash, id->bytes, sizeof(id->bytes));
    }
    return md_hash_finish(&hash);
}

static void md_copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst == 0 || dst_len == 0u) {
        return;
    }
    if (src == 0) {
        src = "";
    }
    snprintf(dst, dst_len, "%s", src);
}

void md_ledger_init(MdLedger *ledger, uint32_t network_id) {
    if (ledger == 0) {
        return;
    }
    memset(ledger, 0, sizeof(*ledger));
    ledger->network_id = network_id;
    ledger->current_epoch = 1u;
}

int md_ledger_asset_index(const MdLedger *ledger, const MdDigest *asset_id) {
    if (ledger == 0 || asset_id == 0) {
        return -1;
    }
    for (size_t i = 0u; i < ledger->asset_count; i++) {
        if (md_digest_equal(&ledger->assets[i].id, asset_id)) {
            return (int)i;
        }
    }
    return -1;
}

MdStatus md_ledger_add_asset(MdLedger *ledger, const char *symbol, uint8_t decimals, MdDigest *out_id) {
    MdAsset *asset;

    if (ledger == 0 || symbol == 0 || symbol[0] == '\0') {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->asset_count >= MD_MAX_ASSETS) {
        return MD_ERR_CAPACITY;
    }
    asset = &ledger->assets[ledger->asset_count];
    memset(asset, 0, sizeof(*asset));
    md_copy_text(asset->symbol, sizeof(asset->symbol), symbol);
    asset->decimals = decimals;
    asset->id = md_digest_text("meridian-asset", asset->symbol);
    if (md_ledger_asset_index(ledger, &asset->id) >= 0) {
        return MD_ERR_DUPLICATE;
    }
    if (out_id != 0) {
        *out_id = asset->id;
    }
    ledger->asset_count++;
    return MD_OK;
}

MdStatus md_ledger_add_account(MdLedger *ledger, const char *label, const char *role, MdDigest *out_id) {
    MdAccount *account;
    MdIdentity identity;

    if (ledger == 0 || label == 0 || label[0] == '\0') {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->account_count >= MD_MAX_ACCOUNTS) {
        return MD_ERR_CAPACITY;
    }
    identity = md_identity_from_label(label);
    if (md_ledger_find_account(ledger, &identity.public_key) != 0) {
        return MD_ERR_DUPLICATE;
    }
    account = &ledger->accounts[ledger->account_count];
    memset(account, 0, sizeof(*account));
    account->id = identity.public_key;
    account->identity = identity;
    md_copy_text(account->role, sizeof(account->role), role);
    account->active = true;
    if (out_id != 0) {
        *out_id = account->id;
    }
    ledger->account_count++;
    return MD_OK;
}

MdStatus md_ledger_add_vault(MdLedger *ledger, const char *label, const MdDigest *asset_id, MdAmount initial_balance, MdDigest *out_id) {
    MdVault *vault;
    int asset_idx;
    MdAmount issued;

    if (ledger == 0 || label == 0 || asset_id == 0 || label[0] == '\0') {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->vault_count >= MD_MAX_VAULTS) {
        return MD_ERR_CAPACITY;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    if (asset_idx < 0) {
        return MD_ERR_NOT_FOUND;
    }
    vault = &ledger->vaults[ledger->vault_count];
    memset(vault, 0, sizeof(*vault));
    md_copy_text(vault->label, sizeof(vault->label), label);
    vault->asset_id = *asset_id;
    vault->id = md_named_digest2("meridian-vault", vault->label, asset_id);
    if (md_ledger_find_vault(ledger, &vault->id) != 0) {
        return MD_ERR_DUPLICATE;
    }
    vault->balance = initial_balance;
    vault->active = true;
    if (md_amount_add(ledger->issued_supply[asset_idx], initial_balance, &issued) != MD_OK) {
        return MD_ERR_POLICY;
    }
    ledger->issued_supply[asset_idx] = issued;
    if (out_id != 0) {
        *out_id = vault->id;
    }
    ledger->vault_count++;
    return MD_OK;
}

MdStatus md_ledger_add_lane(MdLedger *ledger, const char *label, const MdDigest *asset_id, const MdDigest *source_vault, const MdDigest *sink_vault, const MdDigest *terminal_account, MdLaneClass lane_class, uint32_t operator_fee_bps, uint32_t reserve_bps, MdDigest *out_id) {
    MdSettlementLane *lane;
    MdAmount fee_sum;

    if (ledger == 0 || label == 0 || asset_id == 0 || source_vault == 0 || sink_vault == 0 || terminal_account == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->lane_count >= MD_MAX_LANES) {
        return MD_ERR_CAPACITY;
    }
    if (md_amount_add((MdAmount)operator_fee_bps, (MdAmount)reserve_bps, &fee_sum) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (fee_sum > 10000u) {
        return MD_ERR_POLICY;
    }
    if (md_ledger_find_asset(ledger, asset_id) == 0 ||
        md_ledger_find_vault(ledger, source_vault) == 0 ||
        md_ledger_find_vault(ledger, sink_vault) == 0 ||
        md_ledger_find_account(ledger, terminal_account) == 0) {
        return MD_ERR_NOT_FOUND;
    }
    lane = &ledger->lanes[ledger->lane_count];
    memset(lane, 0, sizeof(*lane));
    md_copy_text(lane->label, sizeof(lane->label), label);
    lane->asset_id = *asset_id;
    lane->source_vault = *source_vault;
    lane->sink_vault = *sink_vault;
    lane->terminal_account = *terminal_account;
    lane->lane_class = lane_class;
    lane->operator_fee_bps = operator_fee_bps;
    lane->reserve_bps = reserve_bps;
    lane->active = true;
    lane->id = md_named_digest2("meridian-lane", lane->label, asset_id);
    if (md_ledger_find_lane(ledger, &lane->id) != 0) {
        return MD_ERR_DUPLICATE;
    }
    if (out_id != 0) {
        *out_id = lane->id;
    }
    ledger->lane_count++;
    return MD_OK;
}

MdStatus md_ledger_deposit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount) {
    MdAccount *account;
    int asset_idx;
    MdAmount updated;

    if (ledger == 0 || account_id == 0 || asset_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    account = md_ledger_find_account(ledger, account_id);
    if (asset_idx < 0 || account == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_add(account->balances[asset_idx], amount, &updated) != MD_OK) {
        return MD_ERR_POLICY;
    }
    account->balances[asset_idx] = updated;
    if (md_amount_add(ledger->issued_supply[asset_idx], amount, &updated) != MD_OK) {
        return MD_ERR_POLICY;
    }
    ledger->issued_supply[asset_idx] = updated;
    return md_ledger_append_journal(ledger, ledger->current_epoch, "deposit", account_id, amount);
}

MdStatus md_ledger_debit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount) {
    MdAccount *account;
    int asset_idx;
    MdAmount updated;

    if (ledger == 0 || account_id == 0 || asset_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    account = md_ledger_find_account(ledger, account_id);
    if (asset_idx < 0 || account == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_sub(account->balances[asset_idx], amount, &updated) != MD_OK) {
        return MD_ERR_BALANCE;
    }
    account->balances[asset_idx] = updated;
    return MD_OK;
}

MdStatus md_ledger_credit(MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id, MdAmount amount) {
    MdAccount *account;
    int asset_idx;
    MdAmount updated;

    if (ledger == 0 || account_id == 0 || asset_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    account = md_ledger_find_account(ledger, account_id);
    if (asset_idx < 0 || account == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_add(account->balances[asset_idx], amount, &updated) != MD_OK) {
        return MD_ERR_POLICY;
    }
    account->balances[asset_idx] = updated;
    return MD_OK;
}

MdStatus md_ledger_credit_vault(MdLedger *ledger, const MdDigest *vault_id, MdAmount amount) {
    MdVault *vault;
    MdAmount updated;

    if (ledger == 0 || vault_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    vault = md_ledger_find_vault(ledger, vault_id);
    if (vault == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_add(vault->balance, amount, &updated) != MD_OK) {
        return MD_ERR_POLICY;
    }
    vault->balance = updated;
    return MD_OK;
}

MdStatus md_ledger_debit_vault(MdLedger *ledger, const MdDigest *vault_id, MdAmount amount) {
    MdVault *vault;
    MdAmount updated;

    if (ledger == 0 || vault_id == 0) {
        return MD_ERR_ARGUMENT;
    }
    vault = md_ledger_find_vault(ledger, vault_id);
    if (vault == 0) {
        return MD_ERR_NOT_FOUND;
    }
    if (md_amount_sub(vault->balance, amount, &updated) != MD_OK) {
        return MD_ERR_BALANCE;
    }
    vault->balance = updated;
    return MD_OK;
}

MdAccount *md_ledger_find_account(MdLedger *ledger, const MdDigest *account_id) {
    if (ledger == 0 || account_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->account_count; i++) {
        if (ledger->accounts[i].active && md_digest_equal(&ledger->accounts[i].id, account_id)) {
            return &ledger->accounts[i];
        }
    }
    return 0;
}

const MdAccount *md_ledger_find_account_const(const MdLedger *ledger, const MdDigest *account_id) {
    if (ledger == 0 || account_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->account_count; i++) {
        if (ledger->accounts[i].active && md_digest_equal(&ledger->accounts[i].id, account_id)) {
            return &ledger->accounts[i];
        }
    }
    return 0;
}

MdAsset *md_ledger_find_asset(MdLedger *ledger, const MdDigest *asset_id) {
    if (ledger == 0 || asset_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->asset_count; i++) {
        if (md_digest_equal(&ledger->assets[i].id, asset_id)) {
            return &ledger->assets[i];
        }
    }
    return 0;
}

const MdAsset *md_ledger_find_asset_const(const MdLedger *ledger, const MdDigest *asset_id) {
    if (ledger == 0 || asset_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->asset_count; i++) {
        if (md_digest_equal(&ledger->assets[i].id, asset_id)) {
            return &ledger->assets[i];
        }
    }
    return 0;
}

MdVault *md_ledger_find_vault(MdLedger *ledger, const MdDigest *vault_id) {
    if (ledger == 0 || vault_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->vault_count; i++) {
        if (ledger->vaults[i].active && md_digest_equal(&ledger->vaults[i].id, vault_id)) {
            return &ledger->vaults[i];
        }
    }
    return 0;
}

const MdVault *md_ledger_find_vault_const(const MdLedger *ledger, const MdDigest *vault_id) {
    if (ledger == 0 || vault_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->vault_count; i++) {
        if (ledger->vaults[i].active && md_digest_equal(&ledger->vaults[i].id, vault_id)) {
            return &ledger->vaults[i];
        }
    }
    return 0;
}

MdSettlementLane *md_ledger_find_lane(MdLedger *ledger, const MdDigest *lane_id) {
    if (ledger == 0 || lane_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->lane_count; i++) {
        if (ledger->lanes[i].active && md_digest_equal(&ledger->lanes[i].id, lane_id)) {
            return &ledger->lanes[i];
        }
    }
    return 0;
}

const MdSettlementLane *md_ledger_find_lane_const(const MdLedger *ledger, const MdDigest *lane_id) {
    if (ledger == 0 || lane_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->lane_count; i++) {
        if (ledger->lanes[i].active && md_digest_equal(&ledger->lanes[i].id, lane_id)) {
            return &ledger->lanes[i];
        }
    }
    return 0;
}

MdReservation *md_ledger_find_reservation(MdLedger *ledger, const MdDigest *reservation_id) {
    if (ledger == 0 || reservation_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->reservation_count; i++) {
        if (md_digest_equal(&ledger->reservations[i].reservation_id, reservation_id)) {
            return &ledger->reservations[i];
        }
    }
    return 0;
}

const MdReservation *md_ledger_find_reservation_const(const MdLedger *ledger, const MdDigest *reservation_id) {
    if (ledger == 0 || reservation_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->reservation_count; i++) {
        if (md_digest_equal(&ledger->reservations[i].reservation_id, reservation_id)) {
            return &ledger->reservations[i];
        }
    }
    return 0;
}

MdReceipt *md_ledger_find_receipt(MdLedger *ledger, const MdDigest *receipt_id) {
    if (ledger == 0 || receipt_id == 0) {
        return 0;
    }
    for (size_t i = 0u; i < ledger->receipt_count; i++) {
        if (md_digest_equal(&ledger->receipts[i].receipt_id, receipt_id)) {
            return &ledger->receipts[i];
        }
    }
    return 0;
}

MdStatus md_ledger_append_journal(MdLedger *ledger, uint64_t epoch, const char *event, const MdDigest *subject, MdAmount amount) {
    MdJournalEntry *entry;

    if (ledger == 0 || event == 0) {
        return MD_ERR_ARGUMENT;
    }
    if (ledger->journal_count >= MD_MAX_JOURNAL) {
        return MD_ERR_CAPACITY;
    }
    entry = &ledger->journal[ledger->journal_count];
    memset(entry, 0, sizeof(*entry));
    entry->epoch = epoch;
    snprintf(entry->event, sizeof(entry->event), "%s", event);
    if (subject != 0) {
        entry->subject = *subject;
    } else {
        md_digest_to_hex(&entry->subject);
    }
    entry->amount = amount;
    ledger->journal_count++;
    return MD_OK;
}

MdAmount md_ledger_account_balance(const MdLedger *ledger, const MdDigest *account_id, const MdDigest *asset_id) {
    const MdAccount *account;
    int asset_idx;

    if (ledger == 0 || account_id == 0 || asset_id == 0) {
        return 0u;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    account = md_ledger_find_account_const(ledger, account_id);
    if (asset_idx < 0 || account == 0) {
        return 0u;
    }
    return account->balances[asset_idx];
}

MdAmount md_ledger_vault_balance(const MdLedger *ledger, const MdDigest *vault_id) {
    const MdVault *vault = md_ledger_find_vault_const(ledger, vault_id);

    if (vault == 0) {
        return 0u;
    }
    return vault->balance;
}

MdAmount md_ledger_locked_total(const MdLedger *ledger, const MdDigest *asset_id) {
    MdAmount total = 0u;

    if (ledger == 0 || asset_id == 0) {
        return 0u;
    }
    for (size_t i = 0u; i < ledger->reservation_count; i++) {
        const MdReservation *reservation = &ledger->reservations[i];
        if (reservation->state == MD_RESERVATION_RESERVED && md_digest_equal(&reservation->asset_id, asset_id)) {
            total += reservation->gross_amount;
        }
    }
    return total;
}

MdAmount md_ledger_total_observed(const MdLedger *ledger, const MdDigest *asset_id) {
    MdAmount total = 0u;
    int asset_idx;

    if (ledger == 0 || asset_id == 0) {
        return 0u;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    if (asset_idx < 0) {
        return 0u;
    }
    for (size_t i = 0u; i < ledger->account_count; i++) {
        total += ledger->accounts[i].balances[asset_idx];
    }
    for (size_t i = 0u; i < ledger->vault_count; i++) {
        if (ledger->vaults[i].active && md_digest_equal(&ledger->vaults[i].asset_id, asset_id)) {
            total += ledger->vaults[i].balance;
        }
    }
    total += md_ledger_locked_total(ledger, asset_id);
    return total;
}

bool md_ledger_conservation_ok(const MdLedger *ledger, const MdDigest *asset_id) {
    int asset_idx;

    if (ledger == 0 || asset_id == 0) {
        return false;
    }
    asset_idx = md_ledger_asset_index(ledger, asset_id);
    if (asset_idx < 0) {
        return false;
    }
    return md_ledger_total_observed(ledger, asset_id) == ledger->issued_supply[asset_idx];
}

MdDigest md_ledger_state_digest(const MdLedger *ledger) {
    char payload[32768];
    MdWriter writer;

    if (ledger == 0) {
        return md_digest_text("meridian-ledger-state", "null-ledger");
    }
    md_writer_init(&writer, payload, sizeof(payload));
    md_writer_put(&writer, "ledger{");
    md_writer_u32(&writer, "network", ledger->network_id);
    md_writer_u64(&writer, "epoch", ledger->current_epoch);
    md_writer_u64(&writer, "asset_count", (uint64_t)ledger->asset_count);
    for (size_t i = 0u; i < ledger->asset_count; i++) {
        md_writer_put(&writer, "asset{");
        md_writer_digest(&writer, "id", &ledger->assets[i].id);
        md_writer_field(&writer, "symbol", ledger->assets[i].symbol);
        md_writer_u32(&writer, "decimals", ledger->assets[i].decimals);
        md_writer_amount(&writer, "issued", ledger->issued_supply[i]);
        md_writer_put(&writer, "}");
    }
    md_writer_u64(&writer, "account_count", (uint64_t)ledger->account_count);
    for (size_t i = 0u; i < ledger->account_count; i++) {
        md_writer_put(&writer, "account{");
        md_writer_digest(&writer, "id", &ledger->accounts[i].id);
        md_writer_field(&writer, "role", ledger->accounts[i].role);
        for (size_t a = 0u; a < ledger->asset_count; a++) {
            md_writer_amount(&writer, "balance", ledger->accounts[i].balances[a]);
        }
        md_writer_u64(&writer, "intent_nonce", ledger->accounts[i].intent_nonce);
        md_writer_u64(&writer, "receipt_nonce", ledger->accounts[i].receipt_nonce);
        md_writer_put(&writer, "}");
    }
    md_writer_u64(&writer, "vault_count", (uint64_t)ledger->vault_count);
    for (size_t i = 0u; i < ledger->vault_count; i++) {
        md_writer_put(&writer, "vault{");
        md_writer_digest(&writer, "id", &ledger->vaults[i].id);
        md_writer_digest(&writer, "asset", &ledger->vaults[i].asset_id);
        md_writer_amount(&writer, "balance", ledger->vaults[i].balance);
        md_writer_amount(&writer, "pending_in", ledger->vaults[i].pending_in);
        md_writer_amount(&writer, "pending_out", ledger->vaults[i].pending_out);
        md_writer_put(&writer, "}");
    }
    md_writer_u64(&writer, "reservation_count", (uint64_t)ledger->reservation_count);
    for (size_t i = 0u; i < ledger->reservation_count; i++) {
        md_writer_put(&writer, "reservation{");
        md_writer_digest(&writer, "id", &ledger->reservations[i].reservation_id);
        md_writer_digest(&writer, "intent", &ledger->reservations[i].intent_id);
        md_writer_u32(&writer, "state", (uint32_t)ledger->reservations[i].state);
        md_writer_amount(&writer, "gross", ledger->reservations[i].gross_amount);
        md_writer_digest(&writer, "route", &ledger->reservations[i].route_digest);
        md_writer_put(&writer, "}");
    }
    md_writer_u64(&writer, "receipt_count", (uint64_t)ledger->receipt_count);
    for (size_t i = 0u; i < ledger->receipt_count; i++) {
        md_writer_put(&writer, "receipt{");
        md_writer_digest(&writer, "id", &ledger->receipts[i].receipt_id);
        md_writer_digest(&writer, "lane", &ledger->receipts[i].effective_lane);
        md_writer_digest(&writer, "beneficiary", &ledger->receipts[i].effective_beneficiary);
        md_writer_amount(&writer, "net", ledger->receipts[i].net_amount);
        md_writer_amount(&writer, "fee", ledger->receipts[i].operator_fee);
        md_writer_amount(&writer, "reserve", ledger->receipts[i].reserve_fee);
        md_writer_put(&writer, "}");
    }
    md_writer_put(&writer, "}");
    if (md_writer_status(&writer) != MD_OK) {
        return md_digest_text("meridian-ledger-state", "writer-capacity");
    }
    return md_digest_canonical("meridian-ledger-state", payload);
}

MdStatus md_ledger_lane_economics(const MdLedger *ledger, const MdDigest *lane_id, MdAmount gross_amount, MdLaneEconomics *out) {
    const MdSettlementLane *lane;
    MdAmount total_fee;

    if (ledger == 0 || lane_id == 0 || out == 0) {
        return MD_ERR_ARGUMENT;
    }
    lane = md_ledger_find_lane_const(ledger, lane_id);
    if (lane == 0 || !lane->active) {
        return MD_ERR_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    out->lane_id = lane->id;
    out->terminal_account = lane->terminal_account;
    out->source_vault = lane->source_vault;
    out->sink_vault = lane->sink_vault;
    out->gross_amount = gross_amount;
    out->fee_bps_total = lane->operator_fee_bps + lane->reserve_bps;
    if (md_amount_mul_bps(gross_amount, lane->operator_fee_bps, &out->operator_fee) != MD_OK ||
        md_amount_mul_bps(gross_amount, lane->reserve_bps, &out->reserve_fee) != MD_OK) {
        return MD_ERR_POLICY;
    }
    if (md_amount_add(out->operator_fee, out->reserve_fee, &total_fee) != MD_OK) {
        return MD_ERR_POLICY;
    }
    return md_amount_sub(gross_amount, total_fee, &out->net_amount);
}

MdStatus md_ledger_vault_exposure(const MdLedger *ledger, const MdDigest *vault_id, MdVaultExposure *out) {
    const MdVault *vault;

    if (ledger == 0 || vault_id == 0 || out == 0) {
        return MD_ERR_ARGUMENT;
    }
    vault = md_ledger_find_vault_const(ledger, vault_id);
    if (vault == 0 || !vault->active) {
        return MD_ERR_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    out->vault_id = vault->id;
    out->asset_id = vault->asset_id;
    out->balance = vault->balance;
    out->pending_in = vault->pending_in;
    out->pending_out = vault->pending_out;
    out->available = vault->pending_out > vault->balance ? 0u : vault->balance - vault->pending_out;
    for (size_t i = 0u; i < ledger->lane_count; i++) {
        const MdSettlementLane *lane = &ledger->lanes[i];
        if (!lane->active) {
            continue;
        }
        if (md_digest_equal(&lane->source_vault, vault_id)) {
            out->outbound_lanes++;
        }
        if (md_digest_equal(&lane->sink_vault, vault_id)) {
            out->inbound_lanes++;
        }
    }
    return MD_OK;
}

MdDigest md_lane_economics_digest(const MdLaneEconomics *economics) {
    char payload[1024];
    MdWriter writer;

    if (economics == 0) {
        return md_digest_text("meridian-lane-economics", "null");
    }
    md_writer_init(&writer, payload, sizeof(payload));
    md_writer_put(&writer, "lane_economics{");
    md_writer_digest(&writer, "lane", &economics->lane_id);
    md_writer_digest(&writer, "terminal", &economics->terminal_account);
    md_writer_digest(&writer, "source", &economics->source_vault);
    md_writer_digest(&writer, "sink", &economics->sink_vault);
    md_writer_amount(&writer, "gross", economics->gross_amount);
    md_writer_amount(&writer, "operator_fee", economics->operator_fee);
    md_writer_amount(&writer, "reserve_fee", economics->reserve_fee);
    md_writer_amount(&writer, "net", economics->net_amount);
    md_writer_u32(&writer, "fee_bps", economics->fee_bps_total);
    md_writer_put(&writer, "}");
    if (md_writer_status(&writer) != MD_OK) {
        return md_digest_text("meridian-lane-economics", "invalid");
    }
    return md_digest_canonical("meridian-lane-economics", payload);
}

MdDigest md_vault_exposure_digest(const MdVaultExposure *exposure) {
    char payload[1024];
    MdWriter writer;

    if (exposure == 0) {
        return md_digest_text("meridian-vault-exposure", "null");
    }
    md_writer_init(&writer, payload, sizeof(payload));
    md_writer_put(&writer, "vault_exposure{");
    md_writer_digest(&writer, "vault", &exposure->vault_id);
    md_writer_digest(&writer, "asset", &exposure->asset_id);
    md_writer_amount(&writer, "balance", exposure->balance);
    md_writer_amount(&writer, "pending_in", exposure->pending_in);
    md_writer_amount(&writer, "pending_out", exposure->pending_out);
    md_writer_amount(&writer, "available", exposure->available);
    md_writer_u64(&writer, "inbound", (uint64_t)exposure->inbound_lanes);
    md_writer_u64(&writer, "outbound", (uint64_t)exposure->outbound_lanes);
    md_writer_put(&writer, "}");
    if (md_writer_status(&writer) != MD_OK) {
        return md_digest_text("meridian-vault-exposure", "invalid");
    }
    return md_digest_canonical("meridian-vault-exposure", payload);
}
