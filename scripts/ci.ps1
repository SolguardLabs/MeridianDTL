$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$WslRoot = (& wsl wslpath -a $Root.Path).Trim()
$QuotedRoot = $WslRoot.Replace("'", "'\''")

& wsl bash -lc "cd '$QuotedRoot' && make clean && bash scripts/build.sh && make test-c && ./build/meridiandtl direct >/dev/null && ./build/meridiandtl routed >/dev/null && ./build/meridiandtl epoch-batch >/dev/null && ./build/meridiandtl recompact >/dev/null && ./build/meridiandtl rejections >/dev/null && ./build/meridiandtl operations >/dev/null && ./build/meridiandtl risk-portfolio >/dev/null"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

node scripts/check-js.mjs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

node --test --test-concurrency=1 "tests/node/*.test.js"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

npm run fmt:check
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

npm run scan:public
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

npm run release:check
exit $LASTEXITCODE
