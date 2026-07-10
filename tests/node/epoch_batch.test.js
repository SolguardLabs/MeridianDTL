import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, assertHex32, assertReceipt, scenario } from "../helpers/scenario_helpers.js";

test("epoch batch processes two reservations with independent receipts", () => {
    const payload = scenario("epoch-batch");

    assertCommon(payload, "epoch-batch");
    assertHex32(payload.first_receipt);
    assertHex32(payload.second_receipt);
    assert.notEqual(payload.first_receipt, payload.second_receipt);
    assertReceipt(payload.receipt);
    assert.equal(payload.receipt.id, payload.second_receipt);
    assert.equal(payload.epoch, 6);
    assert.equal(payload.journal.count, 5);
});

test("epoch batch accumulates fees and reserves across settlements", () => {
    const payload = scenario("epoch-batch");

    assert.equal(payload.balances.payer, 860_000);
    assert.equal(payload.balances.merchant, 139_360);
    assert.equal(payload.balances.operator, 440);
    assert.equal(payload.balances.locked, 0);
    assert.equal(payload.vaults.south, 100_200);
});
