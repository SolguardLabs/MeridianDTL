import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, scenario } from "../helpers/scenario_helpers.js";

test("multi-hop route settles on the final priority lane", () => {
    const payload = scenario("routed");

    assertCommon(payload, "routed");
    assert.equal(payload.receipt.gross, 150_000);
    assert.equal(payload.receipt.operator_fee, 600);
    assert.equal(payload.receipt.reserve_fee, 300);
    assert.equal(payload.receipt.net, 149_100);
    assert.equal(payload.balances.payer, 850_000);
    assert.equal(payload.balances.merchant, 149_100);
    assert.equal(payload.balances.operator, 600);
    assert.equal(payload.balances.locked, 0);
    assert.equal(payload.vaults.south, 100_300);
});

test("routed settlement keeps bridge account as operational metadata only", () => {
    const payload = scenario("routed");

    assert.equal(payload.balances.bridge, 0);
    assert.equal(payload.route.removed_hops, 0);
    assert.equal(payload.route.hop_count, 2);
    assert.equal(payload.route.effective_lane, payload.receipt.lane);
});
