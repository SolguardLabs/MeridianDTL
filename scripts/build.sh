#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v make >/dev/null 2>&1; then
    echo "No se encontro make en PATH." >&2
    exit 127
fi

if [[ -z "${CC:-}" ]]; then
    if command -v cc >/dev/null 2>&1; then
        export CC=cc
    elif command -v gcc >/dev/null 2>&1; then
        export CC=gcc
    elif command -v clang >/dev/null 2>&1; then
        export CC=clang
    else
        echo "No se encontro compilador C compatible." >&2
        exit 127
    fi
fi

make all
