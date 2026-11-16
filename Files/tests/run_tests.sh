#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Running Teammate A Storage Layer Tests ==="
if [ -f "$REPO_ROOT/build/Release/test_storage.exe" ]; then
    "$REPO_ROOT/build/Release/test_storage.exe"
elif [ -f "$REPO_ROOT/build/test_storage" ]; then
    "$REPO_ROOT/build/test_storage"
else
    echo "test_storage binary not found. Please build the project first."
    exit 1
fi

echo ""
echo "=== Running Teammate B Snapshots Layer Tests ==="
if [ -f "$REPO_ROOT/build/Release/test_snapshots.exe" ]; then
    "$REPO_ROOT/build/Release/test_snapshots.exe"
elif [ -f "$REPO_ROOT/build/test_snapshots" ]; then
    "$REPO_ROOT/build/test_snapshots"
else
    echo "test_snapshots binary not found. Please build the project first."
    exit 1
fi

echo ""
echo "=== Running Git Parity & End-to-End Tests ==="
python "$SCRIPT_DIR/test_git_parity.py"

echo ""
echo "=== All Tests Passed Successfully! ==="
