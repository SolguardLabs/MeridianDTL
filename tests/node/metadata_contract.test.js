import test from "node:test";
import assert from "node:assert/strict";
import { assertCommon, assertHex32, assertReceipt, scenario } from "../helpers/scenario_helpers.js";

test("direct scenario exposes stable settlement metadata", () => {
    const payload = scenario("direct");

    assertCommon(payload, "direct");
    assertHex32(payload.intent);
    assertHex32(payload.reservation);
    assertReceipt(payload.receipt);
    assert.equal(payload.epoch, 4);
    assert.equal(payload.route.hop_count, 1);
    assert.equal(payload.route.removed_hops, 0);
    assert.equal(payload.route.before, payload.route.after);
});

test("routed scenario exposes route and receipt identifiers", () => {
    const payload = scenario("routed");

    assertCommon(payload, "routed");
    assertHex32(payload.intent);
    assertHex32(payload.reservation);
    assertReceipt(payload.receipt);
    assertHex32(payload.route.before);
    assertHex32(payload.route.after);
    assertHex32(payload.route.effective_lane);
    assert.equal(payload.route.hop_count, 2);
});
