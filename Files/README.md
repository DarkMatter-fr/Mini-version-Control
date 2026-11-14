# Mini Git in C++

A lightweight command-line version control system modeled on Git's internals, developed for **Data Structures Lab (15B17CI371)**, 3rd Semester, JIIT Sector 62 Noida.

---

## Teammate A Implementation: Storage Layer & Objects

This module implements the foundational content-addressable storage engine and immutable object model for Mini Git.

### Components Implemented

1. **SHA-1 & Hex Utilities (`include/sha1.hpp`, `src/sha1.cpp`)**
   - RFC 3174 compliant SHA-1 engine without external dependencies.
   - Raw 20-byte and 40-character lowercase hex digest calculation.
   - Hex encoding and decoding helpers (`toHex`, `fromHex`, `isValidHash`).
   - Verified against standard test vectors and real Git output.

2. **Custom Hash Table (`include/hash_table.hpp`)**
   - Implemented from scratch (no `std::unordered_map` wrapper).
   - Separate chaining with power-of-two bucket count.
   - 64-bit FNV-1a hashing algorithm for strings.
   - Dynamic rehashing when load factor exceeds 0.75.
   - $O(1)$ average time complexity for `insert`, `find`, and `erase`.

3. **Content-Addressable Object Store (`include/object_store.hpp`, `src/object_store.cpp`)**
   - Git framing: `<type> <size>\0<content>`, hashed with SHA-1.
   - On-disk fan-out: `.git/objects/<first-2-hex>/<remaining-38-hex>`.
   - In-memory caching via custom `HashTable` for fast repeated lookups.
   - Idempotent writes: identical content reuses the existing object without duplication.
   - Binary-safe I/O (handles null bytes and arbitrary binary data).
   - Upward repository discovery helper (`findRepoDir`).

4. **Object Model & N-ary Merkle Tree (`include/objects.hpp`, `src/objects.cpp`)**
   - **Blobs:** `writeBlob` and `readBlob`.
   - **Trees:** `TreeEntry` (mode, name, hash), `serializeTree`, and `parseTree`.
     - Git-compliant entry sorting (directory entries compared with trailing `/`).
     - Binary serialization storing raw 20-byte SHA-1 digests.
   - **N-ary Tree (`TreeNode`):** Custom tree structure representing filesystem hierarchies.
     - Bottom-up post-order Merkle tree serialization (`writeTree`), caching subtrees.
     - Top-down pre-order flattening (`flattenTree`) for checkout and status operations.
   - **Commits:** `Commit` struct, `Signature` (name, email, timestamp, timezone), `serializeCommit`, and `parseCommit`.

5. **Commands (`src/commands/init.cpp`, `src/commands/hash_cat.cpp`, `src/main.cpp`)**
   - `git init [dir]`: Sets up `.git/objects/`, `.git/refs/heads/`, and `.git/HEAD` (`ref: refs/heads/main`). Fails cleanly on existing repositories.
   - `git hash-object [-w] [-t <type>] <file>`: Computes object SHA-1 hash and optionally writes it to the object store.
   - `git cat-file (-p | -t | -s) <hash>`: Inspects type, size, or pretty-prints content of stored objects.

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
1. **C++ Unit Tests (`test_storage`):**
   ```bash
   # Windows
   .\build\Release\test_storage.exe

   # Linux / macOS
   ./build/test_storage
   ```

2. **Git Parity Verification (`test_git_parity.py`):**
   Compares our custom `git` implementation against real `git` for identical blobs, trees, and commits:
   ```bash
   python tests/test_git_parity.py
   ```

3. **All Tests Runner:**
   - PowerShell (Windows):
     ```powershell
     powershell -ExecutionPolicy Bypass -File tests/run_tests.ps1
     ```
   - Bash (Linux/macOS):
     ```bash
     bash tests/run_tests.sh
     ```
