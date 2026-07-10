import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, scenario } from "../helpers/scenario_helpers.js";

test("direct lane settles gross amount into net, operator fee and reserve", () => {
    const payload = scenario("direct");

    assertCommon(payload, "direct");
    assert.equal(payload.receipt.gross, 100_000);
    assert.equal(payload.receipt.operator_fee, 250);
    assert.equal(payload.receipt.reserve_fee, 100);
    assert.equal(payload.receipt.net, 99_650);
    assert.equal(payload.balances.payer, 900_000);
    assert.equal(payload.balances.merchant, 99_650);
    assert.equal(payload.balances.operator, 250);
    assert.equal(payload.balances.counterparty, 0);
    assert.equal(payload.balances.locked, 0);
    assert.equal(payload.vaults.south, 100_100);
});

test("direct settlement leaves passive vaults unchanged", () => {
    const payload = scenario("direct");

    assert.equal(payload.vaults.north, 500_000);
    assert.equal(payload.vaults.relay, 250_000);
    assert.equal(payload.vaults.archive, 75_000);
});
