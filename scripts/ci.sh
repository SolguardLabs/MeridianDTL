#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

make clean
bash scripts/build.sh

echo "==> Ejecutando escenarios de humo"
./build/meridiandtl direct >/dev/null
./build/meridiandtl routed >/dev/null
./build/meridiandtl epoch-batch >/dev/null
./build/meridiandtl recompact >/dev/null
./build/meridiandtl rejections >/dev/null
./build/meridiandtl operations >/dev/null

echo "==> Verificando JavaScript"
resolve_node() {
    if command -v node >/dev/null 2>&1 && node --version >/dev/null 2>&1; then
        command -v node
        return 0
    fi
    if command -v node.exe >/dev/null 2>&1 && node.exe --version >/dev/null 2>&1; then
        command -v node.exe
        return 0
    fi
    return 1
}

if ! NODE_BIN="$(resolve_node)"; then
    echo "No se encontro un Node.js ejecutable desde este shell." >&2
    echo "En Windows+WSL usa: powershell -ExecutionPolicy Bypass -File scripts/ci.ps1" >&2
    exit 127
fi

"$NODE_BIN" scripts/check-js.mjs
"$NODE_BIN" --test --test-concurrency=1 "tests/node/*.test.js"
