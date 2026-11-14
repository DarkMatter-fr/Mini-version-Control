# Mini Git Build Checklist

Legend: **[A]** Storage, **[B]** Snapshots, **[C]** History. Owner tags match the team split in the PRD.
Rule: don't start a phase until the previous one passes its verification step.

---

## Phase 0: Setup
- [x] Create repo, `.gitignore` (build/, .git/)
- [x] `CMakeLists.txt` with C++17, warnings on (`-Wall -Wextra`)
- [x] Folder skeleton: `include/`, `src/`, `src/commands/`, `tests/`, `docs/`
- [x] Decide name (`git`) and apply everywhere
- [x] Agree on code style and branch workflow for the team
- [x] **Freeze interfaces:** `objects.hpp`, `object_store.hpp`

## Phase 1: Storage layer (M1) **[A]**
- [x] SHA-1 implementation (or library wrapper)
- [x] Verify SHA-1 against known test vectors ("abc", empty string)
- [x] Custom hash table: insert, find, erase, resize
- [x] Hash table unit tests (collisions, resize, missing keys)
- [x] `writeObject(type, content)` returns hash
- [x] `readObject(hash)` returns type and content
- [x] Objects stored at `objects/ab/cdef...` with directory fan-out
- [x] Writing the same content twice creates no second file
- [x] Binary-mode I/O everywhere
- [x] **Verify:** blob hash equals `git hash-object <file>`
- [x] **Verify:** read-back is byte-identical, including a file with null bytes

## Phase 2: Objects (M2) **[A]**
- [x] Blob serialize / parse
- [x] Tree entry struct (mode, name, hash) with sorted serialization
- [x] N-ary tree node (own implementation) with child list
- [x] Tree serialize / parse (raw 20-byte hashes)
- [x] Commit struct: tree, parents (0..2), author, timestamp, message
- [x] Commit serialize / parse
- [x] **Verify:** tree hash matches Git for a known directory
- [x] **Verify:** commit hash matches Git with identical fields and timestamp

## Phase 3: Init, index, add (M3) **[A/B]**
- [x] `init` creates layout and `HEAD`
- [x] `init` on existing repo fails cleanly
- [ ] Index: load, save, sorted by path
- [ ] Index lookup, insert/update, remove
- [ ] `add <file>` stores blob and updates index
- [ ] `add <dir>` recurses
- [ ] Skip the `.git` directory when walking
- [ ] Re-adding unchanged file creates no new object
- [ ] Error on nonexistent path, index untouched
- [ ] **Verify:** add 3 files, inspect index and objects manually

## Phase 4: Commit and log (M4) **[B/C]**
- [ ] Build tree objects from index, bottom-up (post-order)
- [ ] Reuse hash for unchanged subtrees
- [ ] Write commit with correct parent (none for first commit)
- [ ] Update branch ref (create `main` on first commit)
- [ ] Refuse empty commit ("nothing to commit")
- [ ] Require a non-empty message
- [ ] `log`: walk parents from HEAD, newest first
- [ ] `log` on empty repo prints friendly message
- [ ] **Verify:** 3 commits, log shows all in order
- [ ] **Verify:** change one file, only that path's tree objects are new

## Phase 5: Branch and checkout (M5) **[C/B]**
- [ ] `refs` module: read/write HEAD and branch files
- [ ] Support attached HEAD and detached HEAD
- [ ] `branch` lists branches, marks current
- [ ] `branch <name>` creates ref at current commit
- [ ] Reject duplicate and invalid branch names
- [ ] Flatten commit tree to path -> hash map (pre-order)
- [ ] `checkout <branch>` restores files from target tree
- [ ] Delete tracked files absent from target; remove emptied directories
- [ ] Update index to match the target
- [ ] **Dirty-tree check:** refuse if it would overwrite uncommitted changes
- [ ] `checkout <commit hash>` (detached HEAD)
- [ ] **Verify:** two branches with different files, switch back and forth, contents exact

## Phase 6: Status (M6) **[B]**
- [ ] Compute HEAD tree as sorted path -> hash list
- [ ] Hash working files and compare against the index
- [ ] Report staged (index vs HEAD)
- [ ] Report unstaged modified (working vs index)
- [ ] Report deleted files
- [ ] Report untracked files
- [ ] Clean tree prints "nothing to commit, working tree clean"
- [ ] **Verify:** one file in each state shows up in the right category

## Phase 7: Extended tier (M7) **[C]**
- [ ] Commit graph structure (adjacency by parent hashes)
- [ ] BFS/DFS with visited set
- [ ] `merge-base <a> <b>` via ancestor sets or BFS
- [ ] **Verify:** merge-base on a forked history equals `git merge-base`
- [ ] Fast-forward `merge`
- [ ] Clear message for unsupported non-fast-forward merges
- [ ] `diff`: split into lines
- [ ] LCS dynamic programming table
- [ ] Backtrack to produce +/- output
- [ ] **Verify:** diff output matches `diff -u` semantically on samples

## Phase 8: Optional tier
- [ ] zlib compression on write, decompression on read (CMake option)
- [ ] Hash still computed on uncompressed data
- [ ] 3-way merge with conflict markers
- [ ] Ignore file support

## Phase 9: Testing and hardening
- [ ] `tests/run_tests.sh` runs every scenario, non-zero exit on failure
- [ ] Edge cases: empty file, binary file, nested dirs, empty dir, spaces in names
- [ ] Failure cases: bad paths, duplicate branch, dirty checkout, empty commit
- [ ] No crashes on corrupt or missing objects (clear error instead)
- [ ] Run with `-fsanitize=address,undefined` and fix findings
- [ ] Compare hashes with real Git on a sample repo

## Phase 10: Documentation and submission
- [ ] README: build steps, command usage, examples
- [ ] Architecture diagram (layers, data structure mapping)
- [ ] Update synopsis to match what was actually built
- [ ] Final report: design, data structures, complexity analysis, limitations
- [ ] Demo script: init -> add -> commit -> branch -> checkout -> log -> status
- [ ] Rehearse the demo on a clean machine/clone
- [ ] Each member can explain every module, not just their own

---

## Quick Definition of Done (Core tier)
- [ ] All Core commands work end to end on a fresh repo
- [ ] Hashes match real Git for blobs, trees, commits
- [ ] No known data-loss paths in `checkout`
- [ ] Tests pass; sanitizers clean
- [ ] README lets a stranger build and run it in under 5 minutes