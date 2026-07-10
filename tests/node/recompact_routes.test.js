import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, scenario } from "../helpers/scenario_helpers.js";

test("compatible route hops are compacted before final receipt", () => {
    const payload = scenario("recompact");

    assertCommon(payload, "recompact");
    assert.equal(payload.route.removed_hops, 2);
    assert.equal(payload.route.hop_count, 1);
    assert.notEqual(payload.route.before, payload.route.after);
    assert.equal(payload.route.after, payload.receipt.route_digest);
    assert.equal(payload.route.effective_lane, payload.receipt.lane);
});

test("compacted priority route preserves economic totals", () => {
    const payload = scenario("recompact");

    assert.equal(payload.receipt.gross, 125_000);
    assert.equal(payload.receipt.net, 124_250);
    assert.equal(payload.receipt.operator_fee, 500);
    assert.equal(payload.receipt.reserve_fee, 250);
    assert.equal(payload.balances.payer, 875_000);
    assert.equal(payload.balances.merchant, 124_250);
    assert.equal(payload.vaults.south, 100_250);
});
