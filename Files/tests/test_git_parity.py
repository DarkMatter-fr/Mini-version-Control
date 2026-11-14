import os
import subprocess
import tempfile
import shutil
import sys

def run_cmd(cmd, cwd=None, input_bytes=None):
    p = subprocess.Popen(
        cmd,
        cwd=cwd,
        stdin=subprocess.PIPE if input_bytes is not None else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    stdout, stderr = p.communicate(input=input_bytes)
    return p.returncode, stdout, stderr

def main():
    # Find system git binary
    system_git = shutil.which("git")
    if not system_git or "Mini-version-Control" in os.path.abspath(system_git):
        system_git = r"C:\Program Files\Git\cmd\git.exe"
    assert os.path.exists(system_git), f"System git not found at {system_git}"

    # Find custom mini git binary
    candidates = [
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "Release", "git.exe")),
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "git.exe")),
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "git"))
    ]
    custom_git = next((c for c in candidates if os.path.exists(c)), None)
    assert custom_git is not None, f"Custom git binary not found in candidates: {candidates}"

    print("========================================")
    print("Running Git Parity Verification Tests")
    print(f"System git: {system_git}")
    print(f"Custom git: {custom_git}")
    print("========================================")

    test_dir = tempfile.mkdtemp(prefix="git_parity_")
    try:
        # Test 1: Blob parity across diverse file types
        print("[RUN] Blob parity tests against real Git...")
        sample_files = {
            "empty.txt": b"",
            "hello.txt": b"Hello world!\n",
            "multiline.txt": b"Line 1\nLine 2\nLine 3\n",
            "binary.bin": bytes(range(256)) * 4 + b"\x00\x00\x00"
        }

        for filename, data in sample_files.items():
            filepath = os.path.join(test_dir, filename)
            with open(filepath, "wb") as f:
                f.write(data)

            # System git hash-object
            rc, git_out, _ = run_cmd([system_git, "hash-object", filepath])
            assert rc == 0, f"git hash-object failed for {filename}"
            git_hash = git_out.decode().strip()

            # Custom git hash-object
            rc, custom_out, _ = run_cmd([custom_git, "hash-object", filepath])
            assert rc == 0, f"custom git hash-object failed for {filename}"
            custom_hash = custom_out.decode().strip()

            assert git_hash == custom_hash, f"Hash mismatch for {filename}: real git={git_hash}, custom git={custom_hash}"
            print(f"  [OK] {filename}: {custom_hash}")

        print("[PASS] Blob parity tests passed!")

        # Test 2: CLI init, write object with -w, cat-file (-t, -s, -p)
        print("[RUN] CLI init and cat-file tests...")
        repo_path = os.path.join(test_dir, "test_repo")
        rc, out, err = run_cmd([custom_git, "init", repo_path])
        assert rc == 0, f"git init failed: {err.decode()}"
        assert os.path.isdir(os.path.join(repo_path, ".git", "objects"))
        assert os.path.isdir(os.path.join(repo_path, ".git", "refs", "heads"))
        with open(os.path.join(repo_path, ".git", "HEAD"), "r") as f:
            head_val = f.read().strip()
        assert head_val == "ref: refs/heads/main"

        # Re-init should fail
        rc, _, _ = run_cmd([custom_git, "init", repo_path])
        assert rc != 0, "Re-init should have failed non-zero"

        # Write object into repo
        sample_file = os.path.join(repo_path, "sample.txt")
        sample_bytes = b"Storage layer verified\n"
        with open(sample_file, "wb") as f:
            f.write(sample_bytes)

        rc, out, _ = run_cmd([custom_git, "hash-object", "-w", "sample.txt"], cwd=repo_path)
        assert rc == 0
        written_hash = out.decode().strip()

        # Check fan-out on disk: .git/objects/<2-hex>/<38-hex>
        obj_disk_path = os.path.join(repo_path, ".git", "objects", written_hash[:2], written_hash[2:])
        assert os.path.isfile(obj_disk_path), f"Object not found at fan-out path {obj_disk_path}"

        # cat-file -t
        rc, out, _ = run_cmd([custom_git, "cat-file", "-t", written_hash], cwd=repo_path)
        assert rc == 0
        assert out.decode().strip() == "blob"

        # cat-file -s
        rc, out, _ = run_cmd([custom_git, "cat-file", "-s", written_hash], cwd=repo_path)
        assert rc == 0
        assert int(out.decode().strip()) == len(sample_bytes)

        # cat-file -p
        rc, out, _ = run_cmd([custom_git, "cat-file", "-p", written_hash], cwd=repo_path)
        assert rc == 0
        assert out.strip() == sample_bytes.strip()

        print("[PASS] CLI init and cat-file tests passed!")

        # Test 3: Commit parity against Git
        print("[RUN] Commit hash parity test...")
        tree_hash = "4b825dc642cb6eb9a060e54bf8d69288fbee4904"
        commit_content = (
            f"tree {tree_hash}\n"
            f"author Alice <alice@example.com> 1700000000 +0000\n"
            f"committer Alice <alice@example.com> 1700000000 +0000\n"
            f"\n"
            f"Initial commit\n"
        ).encode()

        commit_file = os.path.join(test_dir, "commit.txt")
        with open(commit_file, "wb") as f:
            f.write(commit_content)

        rc, git_commit_out, _ = run_cmd([system_git, "hash-object", "-t", "commit", commit_file])
        assert rc == 0, "git hash-object -t commit failed"
        git_commit_hash = git_commit_out.decode().strip()

        rc, custom_commit_out, _ = run_cmd([custom_git, "hash-object", "-t", "commit", commit_file])
        assert rc == 0, "custom git hash-object -t commit failed"
        custom_commit_hash = custom_commit_out.decode().strip()

        assert git_commit_hash == custom_commit_hash, f"Commit hash mismatch: real git={git_commit_hash}, custom git={custom_commit_hash}"
        print(f"  [OK] Commit hash matches git: {custom_commit_hash}")
        print("[PASS] Commit hash parity passed!")

        print("========================================")
        print("ALL GIT PARITY TESTS PASSED SUCCESSFULLY")
        print("========================================")

    finally:
        shutil.rmtree(test_dir, ignore_errors=True)

if __name__ == "__main__":
    main()
