import { readdirSync, readFileSync, statSync } from "node:fs";
import { extname, join } from "node:path";

const roots = ["src", "include"];
const extensions = new Set([".c", ".h"]);
let lines = 0;

function walk(directory) {
    for (const entry of readdirSync(directory)) {
        const path = join(directory, entry);
        if (statSync(path).isDirectory()) {
            walk(path);
        } else if (extensions.has(extname(path))) {
            lines += readFileSync(path, "utf8")
                .split(/\r?\n/u)
                .filter((line) => line.trim()).length;
        }
    }
}

roots.forEach(walk);
console.log(`C/H LOC: ${lines}`);
if (lines < 3000) {
    console.error("Expected at least 3000 nonblank C/H lines");
    process.exit(1);
}
