#include "meridian/exposure.h"

#include <assert.h>
#include <stdio.h>

static void test_single_vault_charge(void) {
    MdLedger ledger;
    MdDigest asset;
    MdDigest vault;
    MdCompactedPlan route = {0};
    MdAmount charge = 0u;

    md_ledger_init(&ledger, 7331u);
    assert(md_ledger_add_asset(&ledger, "mUSD", 6u, &asset) == MD_OK);
    assert(md_ledger_add_vault(&ledger, "north-clearing", &asset, 500000u, &vault) == MD_OK);
    route.plan.hop_count = 1u;
    route.plan.hops[0].from_vault = vault;
    route.plan.hops[0].amount_hint = 100000u;
    route.plan.hops[0].weight_bps = 3000u;
    assert(md_route_exposure_charge(&ledger, &route, &charge) == MD_OK);
    assert(charge == 6000u);
}

static void test_capacity_and_weight_bounds(void) {
    MdLedger ledger;
    MdDigest asset;
    MdDigest vault;
    MdCompactedPlan route = {0};
    MdAmount charge = 0u;

    md_ledger_init(&ledger, 7331u);
    assert(md_ledger_add_asset(&ledger, "mUSD", 6u, &asset) == MD_OK);
    assert(md_ledger_add_vault(&ledger, "relay-clearing", &asset, 250000u, &vault) == MD_OK);
    route.plan.hop_count = 1u;
    route.plan.hops[0].from_vault = vault;
    route.plan.hops[0].amount_hint = 100000u;
    route.plan.hops[0].weight_bps = 3000u;
    assert(md_route_exposure_charge(&ledger, &route, &charge) == MD_OK);
    assert(charge == 12000u);
    route.plan.hops[0].amount_hint = 250001u;
    assert(md_route_exposure_charge(&ledger, &route, &charge) == MD_ERR_POLICY);
    route.plan.hops[0].amount_hint = 100000u;
    route.plan.hops[0].weight_bps = 10001u;
    assert(md_route_exposure_charge(&ledger, &route, &charge) == MD_ERR_POLICY);
}

int main(void) {
    test_single_vault_charge();
    test_capacity_and_weight_bounds();
    puts("exposure tests: 2 passed");
    return 0;
}
