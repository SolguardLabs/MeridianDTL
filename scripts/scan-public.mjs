import { readFileSync } from "node:fs";
import { extname } from "node:path";
import { execFileSync } from "node:child_process";

const files = execFileSync("git", ["ls-files", "-co", "--exclude-standard"], { encoding: "utf8" })
    .split(/\r?\n/u)
    .filter(Boolean);
const extensions = new Set([
    ".c",
    ".h",
    ".js",
    ".mjs",
    ".md",
    ".json",
    ".yml",
    ".yaml",
    ".sh",
    ".ps1",
]);
const forbidden = [
    /\bctf\b/iu,
    /\blaborator(?:io|y|ies)\w*\b/iu,
    /\bvulnerab\w*\b/iu,
    /\bexploit\w*\b/iu,
];
const findings = [];

for (const file of files) {
    if (
        file === "LICENSE" ||
        file === "scripts/scan-public.mjs" ||
        !extensions.has(extname(file))
    ) {
        continue;
    }
    readFileSync(file, "utf8")
        .split(/\r?\n/u)
        .forEach((line, index) => {
            if (forbidden.some((pattern) => pattern.test(line))) {
                findings.push(`${file}:${index + 1}`);
            }
        });
}

if (findings.length) {
    console.error(`Public terminology check failed:\n${findings.join("\n")}`);
    process.exit(1);
}
console.log("public terminology check: ok");
