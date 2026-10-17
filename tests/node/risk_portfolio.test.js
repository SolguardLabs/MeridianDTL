import test from "node:test";
import assert from "node:assert/strict";
import { assertHex32, scenario } from "../helpers/scenario_helpers.js";

test("risk portfolio reconciles operating capital components", () => {
    const payload = scenario("risk-portfolio");
    const report = payload.profiles.operating;

    assert.equal(payload.network_id, 7331);
    assert.equal(report.pending_settlement, 1_000_000);
    assert.equal(report.active_positions, 4);
    assert.equal(
        report.required_capital,
        report.loss_charge + report.liquidity_gap + report.fee_reserve + report.concentration_addon,
    );
    assert.ok(report.target_capital > report.required_capital);
    assert.ok(report.capital_surplus > 0);
    assert.equal(report.capital_shortfall, 0);
    assert.equal(report.decision, "approve");
    assertHex32(report.digest);
});

test("severe profile increases capital and places portfolio on hold", () => {
    const payload = scenario("risk-portfolio");
    const operating = payload.profiles.operating;
    const severe = payload.profiles.severe;

    assert.ok(severe.stress_loss > operating.stress_loss);
    assert.ok(severe.liquidity_gap > operating.liquidity_gap);
    assert.ok(severe.required_capital > operating.required_capital);
    assert.ok(severe.target_capital > operating.target_capital);
    assert.ok(severe.capital_shortfall > 0);
    assert.equal(severe.decision, "hold");
    assert.notEqual(severe.digest, operating.digest);
    assertHex32(severe.digest);
});
