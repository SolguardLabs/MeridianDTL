import test from "node:test";
import assert from "node:assert/strict";
import { assertHex32, assertSupplyInvariant, scenario } from "../helpers/scenario_helpers.js";

test("lane economics reports deterministic fee splits", () => {
    const payload = scenario("operations");

    assertSupplyInvariant(payload, "operations");
    assert.equal(payload.lane_economics.direct.gross, 120_000);
    assert.equal(payload.lane_economics.direct.operator_fee, 300);
    assert.equal(payload.lane_economics.direct.reserve_fee, 120);
    assert.equal(payload.lane_economics.direct.net, 119_580);
    assert.equal(payload.lane_economics.direct.fee_bps_total, 35);
    assert.equal(payload.lane_economics.priority.operator_fee, 480);
    assert.equal(payload.lane_economics.priority.reserve_fee, 240);
    assert.equal(payload.lane_economics.priority.net, 119_280);
    assert.equal(payload.lane_economics.priority.fee_bps_total, 60);
    assertHex32(payload.lane_economics.direct.digest);
    assertHex32(payload.lane_economics.priority.digest);
});

test("reservation cancellation clears vault exposure and locked balance", () => {
    const payload = scenario("operations");

    assert.equal(payload.reserve_status, "ok");
    assert.equal(payload.cancel_status, "ok");
    assert.equal(payload.settle_cancelled_status, "state");
    assert.equal(payload.reservation_state, 3);
    assertHex32(payload.reservation);
    assert.equal(payload.exposure_reserved.north.pending_out, 70_000);
    assert.equal(payload.exposure_reserved.north.available, 430_000);
    assert.equal(payload.exposure_reserved.south.pending_in, 70_000);
    assert.equal(payload.exposure_cancelled.north.pending_out, 0);
    assert.equal(payload.exposure_cancelled.north.available, 500_000);
    assert.equal(payload.exposure_cancelled.south.pending_in, 0);
    assert.equal(payload.exposure_before.south.digest, payload.exposure_cancelled.south.digest);
});

test("cancelled reservation restores liquid balances without settlement output", () => {
    const payload = scenario("operations");

    assert.equal(payload.balances.payer, 1_000_000);
    assert.equal(payload.balances.merchant, 0);
    assert.equal(payload.balances.operator, 0);
    assert.equal(payload.balances.locked, 0);
    assert.equal(payload.vaults.north, 500_000);
    assert.equal(payload.vaults.south, 100_000);
    assert.equal(payload.journal.last_event, "cancel");
    assert.equal(payload.journal.count, 3);
});
