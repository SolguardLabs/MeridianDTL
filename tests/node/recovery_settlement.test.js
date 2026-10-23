import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, scenario } from "../helpers/scenario_helpers.js";

test("recovery route settles with exposure reserve and conserved supply", () => {
    const payload = scenario("recovery");

    assertCommon(payload, "recovery");
    assert.equal(payload.receipt.gross, 100_000);
    assert.equal(payload.receipt.operator_fee, 250);
    assert.equal(payload.receipt.reserve_fee, 6_200);
    assert.equal(payload.receipt.net, 93_550);
    assert.equal(
        payload.receipt.gross,
        payload.receipt.operator_fee + payload.receipt.reserve_fee + payload.receipt.net,
    );
    assert.equal(payload.balances.merchant, payload.receipt.net);
    assert.equal(payload.balances.operator, payload.receipt.operator_fee);
    assert.equal(payload.vaults.south, 106_200);
    assert.equal(payload.route.removed_hops, 1);
});
