# Mini Git in C++ (Product Requirements Document)

**Course:** Data Structures Lab (15B17CI371), 3rd Semester, JIIT Sector 62 Noida
**Language / Standard:** C++17
**Status:** Draft v1

---

## 1. Overview

Mini Git is a lightweight command-line version control system modeled on Git's internals. It stores file snapshots in a content-addressable object database, tracks history as a directed acyclic graph (DAG) of immutable commits, and supports branching and restoring past states.

The primary purpose is educational: every core feature maps directly to a fundamental data structure or algorithm.

## 2. Goals

- G1. Implement a working VCS covering init, stage, commit, log, status, branch, checkout.
- G2. Demonstrate data structures through **our own implementations**, not only `std::` containers: hash table, N-ary tree, DAG, sorted index.
- G3. Be verifiable: SHA-1 hashes of blobs, trees, and commits must match real Git's output for identical content.
- G4. Keep a clean modular codebase that three people can develop in parallel.

## 3. Non-Goals

- Networking (clone, push, pull, remotes)
- Packfiles, delta compression, garbage collection
- Full Git CLI compatibility or on-disk compatibility with a real `.git` directory
- Cryptographic security (SHA-1 is used for content addressing, not security)
- Windows/Linux/macOS-specific features beyond what `std::filesystem` provides

## 4. Users

- **Primary:** Course evaluators (need to see data structures and working behavior).
- **Secondary:** The three developers (need clarity and a path to learn Git internals).

## 5. Scope Tiers

| Tier | Contents |
|------|----------|
| **Core (must ship)** | `init`, `add`, `commit`, `log`, `status`, `branch`, `checkout` |
| **Extended** | `merge-base`, fast-forward `merge`, `diff` (LCS) |
| **Optional** | zlib compression, 3-way merge, ignore file |

The Core tier alone is a complete project. Do not start Extended until Core is tested.

## 6. Functional Requirements

### FR-1 `init`
- Creates `.git/objects/`, `.git/refs/heads/`, `.git/HEAD`.
- `HEAD` initially contains `ref: refs/heads/main`.
- Running it in an initialized repo prints an error and exits non-zero without modifying anything.

### FR-2 `add <path>...`
- Reads each file in binary mode, stores it as a blob, updates the index.
- Re-adding an unchanged file creates no new object (deduplication).
- Adding a directory recursively adds all files within it.
- Adding a nonexistent path prints an error and leaves the index unchanged.

### FR-3 `commit -m "<msg>"`
- Builds tree objects recursively from the index (post-order, bottom-up).
- Writes a commit object: tree hash, parent hash (if any), author, timestamp, message.
- Moves the current branch ref to the new commit.
- Refuses to commit when the index matches HEAD's tree ("nothing to commit").
- Unchanged subdirectories reuse the existing tree hash (no duplicate objects).

### FR-4 `log`
- Walks parent pointers from HEAD and prints hash, author, date, message, newest first.
- Must terminate correctly on the root commit and on empty repos (prints a friendly message).
- Extended: handle merge commits with two parents without printing duplicates (visited set).

### FR-5 `status`
- Compares three states: working directory, index, and HEAD tree.
- Reports: staged changes, unstaged modifications, untracked files, deleted files.
- Comparison runs in O(N) over sorted path lists.

### FR-6 `branch [name]`
- No argument: list branches, mark the current one.
- With a name: create a ref pointing at the current commit.
- Reject invalid or duplicate names.

### FR-7 `checkout <branch | commit>`
- Updates `HEAD` and restores working-directory files from the target commit's tree (pre-order traversal).
- Removes tracked files that don't exist in the target.
- **Refuses** if uncommitted changes would be overwritten, with a clear message.
- Updates the index to match the target tree.

### FR-8 (Extended) `merge-base <a> <b>`
- Returns the lowest common ancestor of two commits in the DAG.

### FR-9 (Extended) `merge <branch>`
- Fast-forward when the current branch is an ancestor of the target.
- Otherwise reports "non-fast-forward merge not supported" (Optional tier adds a 3-way merge).

### FR-10 (Extended) `diff`
- Line-based diff between working file and index (or two commits) using LCS dynamic programming.

### FR-11 (Optional) Compression
- Objects are zlib-compressed on write and decompressed on read. The hash is always computed over the **uncompressed** object.

## 7. Data Structure Mapping

| Component | Data structure | Notes |
|-----------|---------------|-------|
| Object database | Custom hash table (in memory) + directory fan-out on disk | `objects/ab/cdef...`; lookup by hash |
| Directory snapshots | N-ary Merkle tree | Internal nodes = trees, leaves = blobs; post-order hashing |
| Commit history | DAG | Nodes = commits; edges point to parents |
| Staging area | Sorted map (std::map or own BST/AVL) | path -> blob hash; enables ordered O(N) comparison |
| `log` / `merge-base` | BFS / DFS with visited set | Merge-base = LCA on the DAG |
| `checkout` | Pre-order tree traversal | Top-down file restoration |
| `commit` | Post-order tree traversal | Bottom-up hash computation |
| `diff` | LCS dynamic programming | Optional/Extended |

**Requirement:** At minimum the hash table and N-ary tree node must be our own implementations, not wrappers over `std::` containers.

## 8. On-Disk Formats

All objects are stored as `<type> <size>\0<content>`, hashed with SHA-1, and stored at `.git/objects/<first 2 hex>/<remaining 38 hex>`.

- **blob:** raw file bytes.
- **tree:** entries sorted by name, each `<mode> <name>\0<20-byte raw hash>` (same as Git, so `git cat-file` can verify).
- **commit:** text:
  ```
  tree <hash>
  parent <hash>        (zero, one, or two lines)
  author <name> <timestamp>

  <message>
  ```
- **index:** text file, one line per entry, sorted by path: `<hash> <path>`.
- **refs:** text file containing a 40-char hash and a newline.
- **HEAD:** `ref: refs/heads/<branch>` or a raw hash (detached).

## 9. Non-Functional Requirements

- **Correctness:** hashes match real Git for identical blobs, trees, and commits.
- **Performance:** a 1,000-file repo commits in under a few seconds; hash lookup is O(1) average.
- **Binary safety:** files are read and written in binary mode; null bytes must not corrupt content.
- **Robustness:** every command validates input and fails with a clear message and non-zero exit code, never a crash.
- **Portability:** builds with CMake and `g++`/`clang++` on C++17; no non-standard dependencies in the Core tier.
- **Maintainability:** one file per command; headers document public interfaces.

## 10. Architecture

```
git/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── sha1.hpp
│   ├── hash_table.hpp
│   ├── object_store.hpp
│   ├── objects.hpp
│   ├── index.hpp
│   ├── refs.hpp
│   ├── commit_graph.hpp
│   └── diff.hpp
├── src/
│   ├── main.cpp
│   ├── sha1.cpp
│   ├── object_store.cpp
│   ├── objects.cpp
│   ├── index.cpp
│   ├── refs.cpp
│   ├── commit_graph.cpp
│   ├── diff.cpp
│   └── commands/   (init, add, commit, log, status, branch, checkout, merge)
├── tests/
└── docs/
```

Layering (lower layers never depend on higher ones):

`commands` -> `index / refs / commit_graph / diff` -> `objects` -> `object_store` -> `sha1 / hash_table`

## 11. Team Split (3 members)

| Area | Modules | Commands |
|------|---------|----------|
| **A: Storage** | sha1, hash_table, object_store, objects | `init`, hash/cat helpers |
| **B: Snapshots** | index, tree building, working-dir ops | `add`, `commit`, `status`, `checkout` |
| **C: History** | refs, commit_graph, diff | `log`, `branch`, `merge-base`, `merge`, `diff` |

**Rule:** freeze the interfaces in `objects.hpp` and `object_store.hpp` before anyone writes commands.

## 12. Milestones

| # | Milestone | Done when |
|---|-----------|-----------|
| M1 | Storage layer | Blob hash matches `git hash-object`; read-back is byte-identical |
| M2 | Objects | Tree and commit hashes match `git cat-file`/`git mktree` output |
| M3 | Staging | `add` works, dedup verified |
| M4 | Commit and log | Multi-commit history prints correctly |
| M5 | Branch and checkout | Switching branches restores exact file contents |
| M6 | Status | All four categories reported correctly |
| M7 | Extended tier | merge-base, fast-forward merge, diff |
| M8 | Polish | Tests, README, optional zlib, final report |

## 13. Verification Plan

- **Hash parity:** for sample files and trees, compare output with real Git.
- **Round trip:** add -> commit -> modify -> checkout restores original bytes (checked by file diff).
- **Edge cases:** empty file, binary file, nested directories, empty directory, filenames with spaces, empty repo, single commit, detached HEAD.
- **Failure cases:** bad paths, duplicate branch, checkout with dirty tree, commit with nothing staged.
- **Test script:** `tests/run_tests.sh` runs all scenarios and exits non-zero on failure.

## 14. Risks

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Tree entry sort order differs from Git | Hash mismatch | Sort by raw name bytes; test early against real Git |
| Text-mode file reads | Corrupt blobs | Binary mode everywhere; test with a binary file |
| Checkout destroys uncommitted work | Data loss | Dirty-tree check before touching files |
| Interface churn between members | Integration pain | Freeze headers at M1/M2 |
| zlib setup issues | Blocked build | Keep Optional; guard with a CMake option |
| Scope creep into merge/diff | Core unfinished | Gate Extended tier behind passing Core tests |

## 15. Deliverables

- Source code with CMake build
- Test script and sample scenarios
- README with build and usage instructions
- Project synopsis and final report
- Demo: init -> add -> commit -> branch -> checkout -> log -> status

## 16. Open Decisions

- [x] Final name: `git` (CLI binary and `.git` directory)
- [ ] Own BST/AVL for the index, or `std::map`
- [ ] Own SHA-1 implementation, or a small library
- [ ] Whether the Optional tier (zlib, 3-way merge) is in scope for submission