import { execFileSync } from "node:child_process";
import { readdirSync, statSync } from "node:fs";
import { join } from "node:path";

const root = process.cwd();
const files = [];

function walk(dir) {
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry);
        const stat = statSync(full);
        if (stat.isDirectory()) {
            walk(full);
        } else if (entry.endsWith(".js") || entry.endsWith(".mjs")) {
            files.push(full);
        }
    }
}

walk(join(root, "tests"));
walk(join(root, "scripts"));

for (const file of files) {
    execFileSync(process.execPath, ["--check", file], { stdio: "pipe" });
}

console.log(`Checked ${files.length} JavaScript modules.`);
