$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

Write-Host "=== Running Teammate A Storage Layer Tests ===" -ForegroundColor Cyan
$testStorage = Join-Path $RepoRoot "build\Release\test_storage.exe"
if (-not (Test-Path $testStorage)) {
    $testStorage = Join-Path $RepoRoot "build\test_storage.exe"
}
if (-not (Test-Path $testStorage)) {
    Write-Error "test_storage executable not found. Please build the project first."
    exit 1
}

& $testStorage
if ($LASTEXITCODE -ne 0) {
    Write-Error "test_storage failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "`n=== Running Teammate B Snapshots Layer Tests ===" -ForegroundColor Cyan
$testSnapshots = Join-Path $RepoRoot "build\Release\test_snapshots.exe"
if (-not (Test-Path $testSnapshots)) {
    $testSnapshots = Join-Path $RepoRoot "build\test_snapshots.exe"
}
if (-not (Test-Path $testSnapshots)) {
    Write-Error "test_snapshots executable not found. Please build the project first."
    exit 1
}

& $testSnapshots
if ($LASTEXITCODE -ne 0) {
    Write-Error "test_snapshots failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "`n=== Running Git Parity & End-to-End Tests ===" -ForegroundColor Cyan
$parityScript = Join-Path $ScriptDir "test_git_parity.py"
python $parityScript
if ($LASTEXITCODE -ne 0) {
    Write-Error "test_git_parity.py failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "`n=== All Tests Passed Successfully! ===" -ForegroundColor Green
