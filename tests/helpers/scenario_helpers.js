import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const root = join(here, "..", "..");
const cache = new Map();
let built = false;

function shellQuote(value) {
    return `'${String(value).replace(/'/g, "'\\''")}'`;
}

function winToWslPath(value) {
    const normalized = value.replaceAll("\\", "/");
    const match = /^([A-Za-z]):\/(.*)$/u.exec(normalized);
    if (!match) {
        return normalized;
    }
    return `/mnt/${match[1].toLowerCase()}/${match[2]}`;
}

function execWsl(script) {
    const candidates = ["wsl", "C:\\Windows\\System32\\wsl.exe"];
    let lastError;
    for (const candidate of candidates) {
        try {
            return execFileSync(candidate, ["bash", "-lc", script], {
                encoding: "utf8",
                timeout: 120_000,
            });
        } catch (error) {
            lastError = error;
            if (error.code !== "ENOENT") {
                throw error;
            }
        }
    }
    throw lastError;
}

export function runInProject(command) {
    if (process.platform === "win32") {
        const wslRoot = winToWslPath(root);
        return execWsl(`cd ${shellQuote(wslRoot)} && ${command}`);
    }
    return execFileSync("bash", ["-lc", command], {
        cwd: root,
        encoding: "utf8",
        timeout: 120_000,
    });
}

export function build() {
    if (!built) {
        runInProject(`
      mkdir -p build
      if [ ! -x build/meridiandtl ]; then
        while ! mkdir build/.build-lock 2>/dev/null; do
          if [ -x build/meridiandtl ]; then
            exit 0
          fi
          sleep 0.1
        done
        trap 'rmdir build/.build-lock' EXIT
        if [ ! -x build/meridiandtl ]; then
          bash scripts/build.sh
        fi
      fi
    `);
        built = true;
    }
}

export function scenario(name) {
    if (!cache.has(name)) {
        build();
        const stdout = runInProject(`./build/meridiandtl ${shellQuote(name)}`);
        cache.set(name, JSON.parse(stdout));
    }
    return cache.get(name);
}

export function scenarioFresh(name) {
    build();
    const stdout = runInProject(`./build/meridiandtl ${shellQuote(name)}`);
    return JSON.parse(stdout);
}

export function assertHex32(value) {
    assert.equal(typeof value, "string");
    assert.match(value, /^[0-9a-f]{64}$/u);
}

export function assertCommon(payload, name) {
    assert.equal(payload.scenario, name);
    assert.equal(payload.network_id, 7331);
    assert.equal(payload.total_supply, 1_925_000);
    assert.equal(payload.observed_supply, 1_925_000);
    assert.equal(payload.conservation_ok, true);
    assertHex32(payload.asset);
    assertHex32(payload.state_digest);
    assert.equal(payload.journal.last_event, "settle");
}

export function assertSupplyInvariant(payload, name) {
    assert.equal(payload.scenario, name);
    assert.equal(payload.network_id, 7331);
    assert.equal(payload.total_supply, 1_925_000);
    assert.equal(payload.observed_supply, 1_925_000);
    assert.equal(payload.conservation_ok, true);
    assertHex32(payload.asset);
    assertHex32(payload.state_digest);
}

export function assertReceipt(receipt) {
    assert.equal(typeof receipt, "object");
    assertHex32(receipt.id);
    assertHex32(receipt.lane);
    assertHex32(receipt.beneficiary);
    assertHex32(receipt.route_digest);
    assert.equal(receipt.gross, receipt.net + receipt.operator_fee + receipt.reserve_fee);
}
