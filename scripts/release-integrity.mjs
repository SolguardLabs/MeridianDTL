import { createHash } from "node:crypto";
import { existsSync, readFileSync, statSync } from "node:fs";
import { execFileSync } from "node:child_process";

const required = [
    "README.md",
    "SECURITY.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "assets/banner.png",
    "docs/arquitectura.md",
    "docs/modelo-economico.md",
    "docs/riesgo-y-capital.md",
    "docs/operacion.md",
    "docs/integracion.md",
];
for (const file of required) {
    if (!existsSync(file) || statSync(file).size === 0) {
        throw new Error(`Missing release artifact: ${file}`);
    }
}

const protectedSources = {
    "src/intent.c": "32879b24d134b0aeeb0ceff2ebc4531dc8cd60f4",
    "src/route.c": "41e6e956506d1afdd3e4de8295e72faaf75dfef4",
    "src/ledger.c": "20c532387ec0ced88ec57af0accf9e491d5d8bee",
};
for (const [file, expected] of Object.entries(protectedSources)) {
    const actual = execFileSync("git", ["hash-object", file], { encoding: "utf8" }).trim();
    if (actual !== expected) {
        throw new Error(`Protected source mismatch: ${file}`);
    }
}

const pkg = JSON.parse(readFileSync("package.json", "utf8"));
if (pkg.version !== "1.0.0") {
    throw new Error(`Unexpected package version: ${pkg.version}`);
}
const banner = readFileSync("assets/banner.png");
if (banner.length < 100_000 || banner.subarray(1, 4).toString("ascii") !== "PNG") {
    throw new Error("Banner is not a valid production PNG");
}
if (!readFileSync("README.md", "utf8").includes("assets/banner.png")) {
    throw new Error("README does not reference the canonical banner");
}
const ref = process.env.GITHUB_REF_NAME ?? "";
if (process.env.GITHUB_REF_TYPE === "tag" && ref !== `v${pkg.version}`) {
    throw new Error(`Tag ${ref} does not match package version ${pkg.version}`);
}
console.log(`release integrity: ok (${createHash("sha256").update(banner).digest("hex")})`);
