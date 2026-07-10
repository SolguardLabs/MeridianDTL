import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, assertHex32, scenario } from "../helpers/scenario_helpers.js";

test("reservation lifecycle rejects expired, duplicate and early actions", () => {
    const payload = scenario("rejections");

    assertCommon(payload, "rejections");
    assert.equal(payload.expired_status, "expired");
    assert.equal(payload.reserve_status, "ok");
    assert.equal(payload.duplicate_status, "duplicate");
    assert.equal(payload.early_settle_status, "policy");
    assert.equal(payload.settle_status, "ok");
    assertHex32(payload.receipt);
});

test("accepted reservation in rejection scenario still settles conservatively", () => {
    const payload = scenario("rejections");

    assert.equal(payload.balances.payer, 950_000);
    assert.equal(payload.balances.merchant, 49_825);
    assert.equal(payload.balances.operator, 125);
    assert.equal(payload.balances.locked, 0);
    assert.equal(payload.vaults.south, 100_050);
});
