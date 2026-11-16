#include "commands.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"
#include "sha1.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond << "\n"; \
            std::exit(1);                                                                \
        }                                                                                \
    } while (0)

namespace {

void writeTestFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
}

std::string readTestFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    return content;
}

}  // namespace

void testIndex() {
    std::cout << "[RUN] testIndex\n";

    Index idx;
    TEST_ASSERT(idx.empty());
    TEST_ASSERT(idx.size() == 0);

    idx.add("src/main.cpp", "1111111111111111111111111111111111111111");
    idx.add("README.md", "2222222222222222222222222222222222222222");
    idx.add("include/util.h", "3333333333333333333333333333333333333333");

    TEST_ASSERT(idx.size() == 3);
    TEST_ASSERT(idx.contains("README.md"));
    TEST_ASSERT(!idx.contains("nonexistent.txt"));
    TEST_ASSERT(idx.getHash("README.md").value() == "2222222222222222222222222222222222222222");

    // Save to disk
    std::filesystem::path testIdxPath = std::filesystem::temp_directory_path() / "test_git_index";
    std::error_code ec;
    std::filesystem::remove(testIdxPath, ec);
    idx.save(testIdxPath);

    // Verify file contents are sorted by path
    std::ifstream in(testIdxPath);
    std::string line1, line2, line3;
    std::getline(in, line1);
    std::getline(in, line2);
    std::getline(in, line3);
    in.close();

    TEST_ASSERT(line1 == "2222222222222222222222222222222222222222 README.md");
    TEST_ASSERT(line2 == "3333333333333333333333333333333333333333 include/util.h");
    TEST_ASSERT(line3 == "1111111111111111111111111111111111111111 src/main.cpp");

    // Load back into new index object
    Index reloaded;
    TEST_ASSERT(reloaded.load(testIdxPath));
    TEST_ASSERT(reloaded.size() == 3);
    TEST_ASSERT(reloaded.getHash("src/main.cpp").value() == "1111111111111111111111111111111111111111");

    // Remove entry
    TEST_ASSERT(reloaded.remove("README.md"));
    TEST_ASSERT(!reloaded.contains("README.md"));
    TEST_ASSERT(reloaded.size() == 2);

    std::filesystem::remove(testIdxPath, ec);
    std::cout << "[PASS] testIndex\n";
}

void testRefs() {
    std::cout << "[RUN] testRefs\n";

    std::filesystem::path testRepo = std::filesystem::temp_directory_path() / "test_git_refs_repo" / ".git";
    std::error_code ec;
    std::filesystem::remove_all(testRepo.parent_path(), ec);
    std::filesystem::create_directories(testRepo / "refs" / "heads", ec);

    // Initial HEAD: ref: refs/heads/main
    refs::setHeadToBranch(testRepo, "main");
    TEST_ASSERT(refs::isHeadSymbolic(testRepo));
    TEST_ASSERT(refs::currentBranch(testRepo).value() == "main");
    TEST_ASSERT(!refs::resolveHead(testRepo).has_value());  // Branch ref doesn't exist yet

    // Write commit to branch
    std::string dummyCommit = "1234567890abcdef1234567890abcdef12345678";
    refs::writeBranch(testRepo, "main", dummyCommit);
    TEST_ASSERT(refs::readBranch(testRepo, "main").value() == dummyCommit);
    TEST_ASSERT(refs::resolveHead(testRepo).value() == dummyCommit);

    // List branches
    refs::writeBranch(testRepo, "feature", dummyCommit);
    auto branches = refs::listBranches(testRepo);
    TEST_ASSERT(branches.size() == 2);
    TEST_ASSERT(branches[0] == "feature");
    TEST_ASSERT(branches[1] == "main");

    // Detached HEAD
    std::string detachedCommit = "abcdefabcdefabcdefabcdefabcdefabcdefabcd";
    refs::setHeadDetached(testRepo, detachedCommit);
    TEST_ASSERT(!refs::isHeadSymbolic(testRepo));
    TEST_ASSERT(!refs::currentBranch(testRepo).has_value());
    TEST_ASSERT(refs::resolveHead(testRepo).value() == detachedCommit);

    std::filesystem::remove_all(testRepo.parent_path(), ec);
    std::cout << "[PASS] testRefs\n";
}

void testAddAndCommit() {
    std::cout << "[RUN] testAddAndCommit\n";

    std::filesystem::path repoRoot = std::filesystem::temp_directory_path() / "test_repo_snapshots";
    std::error_code ec;
    std::filesystem::remove_all(repoRoot, ec);
    std::filesystem::create_directories(repoRoot, ec);

    // 1. Initialize repo
    TEST_ASSERT(cmdInit({repoRoot.string()}) == 0);

    // Change current directory to repoRoot during the test
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);

    // 2. Add nonexistent path -> fails cleanly, index untouched
    TEST_ASSERT(cmdAdd({"does_not_exist.txt"}) != 0);
    Index idxCheck;
    idxCheck.load(repoRoot / ".git" / "index");
    TEST_ASSERT(idxCheck.empty());

    // 3. Create sample files
    writeTestFile(repoRoot / "hello.txt", "Hello world\n");
    writeTestFile(repoRoot / "src" / "code.cpp", "int main() { return 0; }\n");
    writeTestFile(repoRoot / "src" / "header.h", "#pragma once\n");

    // 4. Test git add
    TEST_ASSERT(cmdAdd({"hello.txt", "src"}) == 0);

    Index idx;
    idx.load(repoRoot / ".git" / "index");
    TEST_ASSERT(idx.size() == 3);
    TEST_ASSERT(idx.contains("hello.txt"));
    TEST_ASSERT(idx.contains("src/code.cpp"));
    TEST_ASSERT(idx.contains("src/header.h"));

    // Verify blobs were written to .git/objects
    ObjectStore store(repoRoot / ".git");
    std::string helloHash = idx.getHash("hello.txt").value();
    TEST_ASSERT(store.exists(helloHash));
    TEST_ASSERT(readBlob(store, helloHash) == "Hello world\n");

    // 5. Test commit with empty message -> fails
    TEST_ASSERT(cmdCommit({"-m", ""}) != 0);

    // 6. Test initial commit
    TEST_ASSERT(cmdCommit({"-m", "Initial commit"}) == 0);

    // Verify commit in main ref
    auto commitHash1 = refs::resolveHead(repoRoot / ".git");
    TEST_ASSERT(commitHash1.has_value());
    Commit c1 = readCommit(store, *commitHash1);
    TEST_ASSERT(c1.message == "Initial commit\n");
    TEST_ASSERT(c1.parents.empty());  // root commit has no parents

    // Verify root tree
    auto flat1 = flattenTree(store, c1.tree);
    TEST_ASSERT(flat1.size() == 3);

    // 7. Commit again without changes -> refuses with "nothing to commit"
    TEST_ASSERT(cmdCommit({"-m", "Second commit"}) != 0);

    // 8. Modify a file and commit
    writeTestFile(repoRoot / "hello.txt", "Hello world modified\n");
    TEST_ASSERT(cmdAdd({"hello.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Modify hello"}) == 0);

    auto commitHash2 = refs::resolveHead(repoRoot / ".git");
    TEST_ASSERT(commitHash2.has_value() && *commitHash2 != *commitHash1);
    Commit c2 = readCommit(store, *commitHash2);
    TEST_ASSERT(c2.parents.size() == 1 && c2.parents[0] == *commitHash1);

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(repoRoot, ec);
    std::cout << "[PASS] testAddAndCommit\n";
}

void testStatusAndCheckout() {
    std::cout << "[RUN] testStatusAndCheckout\n";

    std::filesystem::path repoRoot = std::filesystem::temp_directory_path() / "test_repo_checkout";
    std::error_code ec;
    std::filesystem::remove_all(repoRoot, ec);
    std::filesystem::create_directories(repoRoot, ec);

    TEST_ASSERT(cmdInit({repoRoot.string()}) == 0);
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);

    // 1. Initial status on empty repo
    TEST_ASSERT(cmdStatus({}) == 0);

    // 2. Create file A on main branch
    writeTestFile(repoRoot / "fileA.txt", "Content A\n");
    TEST_ASSERT(cmdAdd({"fileA.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Commit on main"}) == 0);

    auto mainCommit = refs::resolveHead(repoRoot / ".git");
    TEST_ASSERT(mainCommit.has_value());

    // 3. Create new branch 'v2' pointing at current commit
    refs::writeBranch(repoRoot / ".git", "v2", *mainCommit);
    TEST_ASSERT(cmdCheckout({"v2"}) == 0);
    TEST_ASSERT(refs::currentBranch(repoRoot / ".git").value() == "v2");

    // 4. Add file B and remove file A on branch 'v2'
    writeTestFile(repoRoot / "fileB.txt", "Content B\n");
    std::filesystem::remove(repoRoot / "fileA.txt", ec);

    Index idx;
    idx.load(repoRoot / ".git" / "index");
    idx.remove("fileA.txt");
    idx.save(repoRoot / ".git" / "index");

    TEST_ASSERT(cmdAdd({"fileB.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Add fileB on v2"}) == 0);

    TEST_ASSERT(!std::filesystem::exists(repoRoot / "fileA.txt"));
    TEST_ASSERT(std::filesystem::exists(repoRoot / "fileB.txt"));

    // 5. Checkout back to 'main'
    TEST_ASSERT(cmdCheckout({"main"}) == 0);
    TEST_ASSERT(refs::currentBranch(repoRoot / ".git").value() == "main");

    // fileA.txt should be restored, fileB.txt should be removed!
    TEST_ASSERT(std::filesystem::exists(repoRoot / "fileA.txt"));
    TEST_ASSERT(readTestFile(repoRoot / "fileA.txt") == "Content A\n");
    TEST_ASSERT(!std::filesystem::exists(repoRoot / "fileB.txt"));

    // 6. Test Dirty-tree check: local uncommitted changes prevent overwrite
    writeTestFile(repoRoot / "fileB.txt", "Dirty untracked local file\n");
    // Switching to v2 would overwrite fileB.txt -> must refuse!
    TEST_ASSERT(cmdCheckout({"v2"}) != 0);
    TEST_ASSERT(readTestFile(repoRoot / "fileB.txt") == "Dirty untracked local file\n");

    std::filesystem::remove(repoRoot / "fileB.txt", ec);
    TEST_ASSERT(cmdCheckout({"v2"}) == 0);
    TEST_ASSERT(readTestFile(repoRoot / "fileB.txt") == "Content B\n");

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(repoRoot, ec);
    std::cout << "[PASS] testStatusAndCheckout\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running Teammate B (Snapshots) Tests\n";
    std::cout << "========================================\n";

    testIndex();
    testRefs();
    testAddAndCommit();
    testStatusAndCheckout();

    std::cout << "========================================\n";
    std::cout << "ALL TEAMMATE B TESTS PASSED SUCCESSFULLY\n";
    std::cout << "========================================\n";
    return 0;
}
