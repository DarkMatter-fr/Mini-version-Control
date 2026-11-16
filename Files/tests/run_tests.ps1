$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

Write-Host "=== Running Teammate A Storage Layer Tests ===" -ForegroundColor Cyan
$testStorage = Join-Path $RepoRoot "build\Release\test_storage.exe"
if (-not (Test-Path $testStorage)) { $testStorage = Join-Path $RepoRoot "build\test_storage.exe" }
& $testStorage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n=== Running Teammate B Snapshots Layer Tests ===" -ForegroundColor Cyan
$testSnapshots = Join-Path $RepoRoot "build\Release\test_snapshots.exe"
if (-not (Test-Path $testSnapshots)) { $testSnapshots = Join-Path $RepoRoot "build\test_snapshots.exe" }
& $testSnapshots
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n=== Running Teammate C History & Diff Layer Tests ===" -ForegroundColor Cyan
$testHistory = Join-Path $RepoRoot "build\Release\test_history.exe"
if (-not (Test-Path $testHistory)) { $testHistory = Join-Path $RepoRoot "build\test_history.exe" }
& $testHistory
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n=== Running Git Parity & End-to-End Tests ===" -ForegroundColor Cyan
$parityScript = Join-Path $ScriptDir "test_git_parity.py"
python $parityScript
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n=== All Tests Passed Successfully! ===" -ForegroundColor Green
