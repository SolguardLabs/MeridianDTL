import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { build, runInProject, scenario } from "../helpers/scenario_helpers.js";

test("default CLI scenario is routed", () => {
    build();
    const payload = JSON.parse(runInProject("./build/meridiandtl"));
    const routed = scenario("routed");

    assert.equal(payload.scenario, "routed");
    assert.equal(payload.state_digest, routed.state_digest);
});

test("unknown scenario exits with a non-zero status", () => {
    build();
    assert.throws(
        () => {
            runInProject("./build/meridiandtl unknown-scenario 2>/dev/null");
        },
        (error) => {
            assert.equal(typeof error.status, "number");
            assert.notEqual(error.status, 0);
            return true;
        },
    );
});

test("node child process can parse a scenario through helper output", () => {
    const stdout = execFileSync(
        process.execPath,
        ["--input-type=module", "-e", "console.log(JSON.stringify({ok:true}))"],
        {
            encoding: "utf8",
        },
    );

    assert.deepEqual(JSON.parse(stdout), { ok: true });
});
