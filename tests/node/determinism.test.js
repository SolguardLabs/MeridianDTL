import test from "node:test";
import assert from "node:assert/strict";
import { scenarioFresh } from "../helpers/scenario_helpers.js";

test("routed scenario is deterministic across independent process runs", () => {
    const first = scenarioFresh("routed");
    const second = scenarioFresh("routed");

    assert.equal(first.state_digest, second.state_digest);
    assert.equal(first.intent, second.intent);
    assert.equal(first.reservation, second.reservation);
    assert.equal(first.receipt.id, second.receipt.id);
    assert.deepEqual(first.balances, second.balances);
    assert.deepEqual(first.vaults, second.vaults);
});

test("route compaction digest is deterministic", () => {
    const first = scenarioFresh("recompact");
    const second = scenarioFresh("recompact");

    assert.equal(first.route.before, second.route.before);
    assert.equal(first.route.after, second.route.after);
    assert.equal(first.receipt.route_digest, second.receipt.route_digest);
});
