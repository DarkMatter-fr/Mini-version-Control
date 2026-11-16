# Mini Git in C++

A lightweight command-line version control system modeled on Git's internals, developed for **Data Structures Lab (15B17CI371)**, 3rd Semester, JIIT Sector 62 Noida.

---

## Architecture & Implementation Overview

### 1. Teammate A: Storage Layer & Objects
* **SHA-1 & Hex Utilities (`include/sha1.hpp`, `src/sha1.cpp`)**: RFC 3174 SHA-1 implementation, 20-byte raw / 40-char hex digest, validation and conversion helpers.
* **Custom Hash Table (`include/hash_table.hpp`)**: Separate chaining, 64-bit FNV-1a hashing, dynamic rehashing at load factor $> 0.75$, $O(1)$ average operations.
* **Content-Addressable Object Database (`include/object_store.hpp`, `src/object_store.cpp`)**: Format `<type> <size>\0<content>`, 2-level directory fan-out (`.git/objects/ab/cdef...`), in-memory caching, idempotency/deduplication, binary-safe I/O.
* **Objects Model (`include/objects.hpp`, `src/objects.cpp`)**:
  - Blobs: `writeBlob`, `readBlob`.
  - Trees: `TreeEntry` with Git sort order (directories compare as `name/`), binary serialization (`serializeTree`, `parseTree`).
  - N-ary Tree Node (`TreeNode`): Merkle tree construction (`writeTree`), pre-order flattening (`flattenTree`).
  - Commits: `Commit`, `Signature`, `serializeCommit`, `parseCommit`.
* **Plumbing Commands**:
  - `git init [dir]`: Initializes `.git/objects/`, `.git/refs/heads/`, and `.git/HEAD`.
  - `git hash-object [-w] [-t <type>] <file>`: Computes object ID and optionally writes to database.
  - `git cat-file (-p | -t | -s) <hash>`: Inspects type, size, or content of an object.

### 2. Teammate B: Snapshots, Indexing & Working Directory Operations
* **Staging Area / Index (`include/index.hpp`, `src/index.cpp`)**:
  - On-disk format: `<40-char-hash> <path>\n` stored at `.git/index`.
  - In-memory `std::map<string, string>` keeping entries strictly sorted by path.
  - Supports load, save, add/update, remove, lookup.
* **Reference Management (`include/refs.hpp`, `src/refs.cpp`)**:
  - Symbolic HEAD (`ref: refs/heads/<branch>`) and detached HEAD resolution.
  - Reading and writing branch ref files in `.git/refs/heads/<branch>`.
* **Snapshots Commands**:
  - `git add <path>...` (`src/commands/add.cpp`): Reads files in binary mode, stores blobs in `ObjectStore`, handles recursive directory traversal, skips `.git/`, updates index.
  - `git commit -m "<msg>"` (`src/commands/commit.cpp`): Builds Merkle tree bottom-up from index via `writeTree`, creates commit object, updates branch ref, rejects empty messages and redundant commits ("nothing to commit").
  - `git status` (`src/commands/status.cpp`): $O(N)$ comparison across HEAD commit tree, index, and working directory. Reports staged changes (new, modified, deleted), unstaged modifications (modified, deleted), and untracked files.
  - `git checkout <branch | commit>` (`src/commands/checkout.cpp`): Restores working tree from target commit tree, updates index, deletes removed files, updates HEAD. Includes **dirty-tree safety checks** preventing uncommitted or untracked changes from being overwritten.

### 3. Teammate C: History, DAG, Merging & Diffing
* **Commit Graph & DAG Traversal (`include/commit_graph.hpp`, `src/commit_graph.cpp`)**:
  - Direct Acyclic Graph (DAG) model of commits linked by parent pointers.
  - History traversal walking commits in newest-first topological/timestamp order.
  - Lowest Common Ancestor (LCA) discovery using Breadth-First Search (BFS) reachability for `merge-base`.
* **Diff Engine & Dynamic Programming LCS (`include/diff.hpp`, `src/diff.cpp`)**:
  - $O(M \times N)$ Longest Common Subsequence (LCS) dynamic programming matrix.
  - Backtracking line-by-line edit script generation (`diffLines`).
  - Unified diff formatter with `--- a/file`, `+++ b/file`, `@@ -start,len +start,len @@` hunk headers, additions (`+`), deletions (`-`), and context (` `).
* **History & Branching Commands**:
  - `git branch [-d <branch>] [<branch>]` (`src/commands/branch.cpp`): List existing branches with `*` indicating the active branch, create new branch pointing to current HEAD, or safely delete a branch with `-d`.
  - `git log` (`src/commands/log.cpp`): Formatted traversal of commits from HEAD backwards, outputting commit hash, author, date, and commit message.
  - `git merge-base <a> <b>` (`src/commands/merge.cpp`): Computes and outputs the lowest common ancestor commit hash between two branches/commits.
  - `git merge <branch>` (`src/commands/merge.cpp`): Merges the specified branch into the current branch. Supports Fast-Forward merges updating working tree, index, and branch ref.
  - `git diff [<commit1> [<commit2>]]` (`src/commands/diff.cpp`): Line-by-line diff between working directory and staging index, or between two arbitrary commits.

---

## Building the Project

### Requirements
- C++17 compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.15+

### Build Commands

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build --config Release
```

The compiled binary will be placed at:
- Windows: `build/Release/git.exe`
- Linux / macOS: `build/git`

---

## Running Tests

### Automated Test Suites
1. **Teammate A Storage Layer Unit Tests (`test_storage`):**
   ```bash
   .\build\Release\test_storage.exe
   ```

2. **Teammate B Snapshots Layer Unit Tests (`test_snapshots`):**
   ```bash
   .\build\Release\test_snapshots.exe
   ```

3. **Teammate C History & DAG Layer Unit Tests (`test_history`):**
   ```bash
   .\build\Release\test_history.exe
   ```

4. **Git Parity & End-to-End Verification (`test_git_parity.py`):**
   ```bash
   python tests/test_git_parity.py
   ```

5. **All Tests Runner:**
   - PowerShell (Windows):
     ```powershell
     powershell -ExecutionPolicy Bypass -File tests/run_tests.ps1
     ```
   - Bash (Linux/macOS):
     ```bash
     bash tests/run_tests.sh
     ```
