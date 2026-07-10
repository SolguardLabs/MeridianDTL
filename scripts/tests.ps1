$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$WslRoot = (& wsl wslpath -a $Root.Path).Trim()

& wsl bash -lc "cd '$($WslRoot.Replace("'", "'\''"))' && bash scripts/build.sh"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

node scripts/check-js.mjs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

node --test --test-concurrency=1 "tests/node/*.test.js"
exit $LASTEXITCODE
