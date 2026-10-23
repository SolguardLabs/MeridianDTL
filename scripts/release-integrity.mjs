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
    "docs/recuperacion-y-exposicion.md",
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
    "src/intent.c": "6263ca047a390a5fa315f1426fa6be5deb331f39",
    "src/exposure.c": "4fc918c430216125063705c215ef91efceef806f",
    "src/route.c": "93b55718eda37d0620d9213a803a82937df0c2db",
    "src/ledger.c": "20c532387ec0ced88ec57af0accf9e491d5d8bee",
};
for (const [file, expected] of Object.entries(protectedSources)) {
    const actual = execFileSync("git", ["hash-object", file], { encoding: "utf8" }).trim();
    if (actual !== expected) {
        throw new Error(`Protected source mismatch: ${file}`);
    }
}

const pkg = JSON.parse(readFileSync("package.json", "utf8"));
if (pkg.version !== "2.0.0") {
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
