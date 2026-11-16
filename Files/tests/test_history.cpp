#include "commands.hpp"
#include "commit_graph.hpp"
#include "diff.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"

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

void testDiffLcs() {
    std::cout << "[RUN] testDiffLcs\n";

    // 1. Identical texts -> empty diff
    std::string textA = "Line 1\nLine 2\nLine 3\n";
    std::string textB = "Line 1\nLine 2\nLine 3\n";
    TEST_ASSERT(formatUnifiedDiff("a/f", "b/f", textA, textB) == "");

    // 2. Added and modified lines
    std::string text1 = "A\nB\nC\n";
    std::string text2 = "A\nB modified\nC\nD added\n";
    auto diff = computeLineDiff(text1, text2);

    bool foundModifiedDel = false;
    bool foundModifiedAdd = false;
    bool foundDAdd = false;

    for (const auto& dl : diff) {
        if (dl.op == DiffOp::DELETE && dl.text == "B") foundModifiedDel = true;
        if (dl.op == DiffOp::ADD && dl.text == "B modified") foundModifiedAdd = true;
        if (dl.op == DiffOp::ADD && dl.text == "D added") foundDAdd = true;
    }

    TEST_ASSERT(foundModifiedDel);
    TEST_ASSERT(foundModifiedAdd);
    TEST_ASSERT(foundDAdd);

    // 3. Formatted unified diff header
    std::string unified = formatUnifiedDiff("a/file.txt", "b/file.txt", text1, text2);
    TEST_ASSERT(unified.find("--- a/file.txt\n+++ b/file.txt\n") != std::string::npos);

    std::cout << "[PASS] testDiffLcs\n";
}

void testCommitGraphAndMergeBase() {
    std::cout << "[RUN] testCommitGraphAndMergeBase\n";

    std::filesystem::path testRepo = std::filesystem::temp_directory_path() / "test_commit_graph_repo";
    std::error_code ec;
    std::filesystem::remove_all(testRepo, ec);
    std::filesystem::create_directories(testRepo / "objects", ec);

    ObjectStore store(testRepo);
    CommitGraph graph(store);

    // Build a DAG:
    // root -> commit1 -> commit2 (branch main)
    //        \-> commit3 (branch feature)

    // Root commit
    Commit cRoot;
    cRoot.tree = "1111111111111111111111111111111111111111";
    cRoot.author = {"Dev", "dev@test.com", 1000, "+0000"};
    cRoot.committer = cRoot.author;
    cRoot.message = "Root\n";
    std::string hRoot = writeCommit(store, cRoot);

    // Commit 1 (child of root)
    Commit c1;
    c1.tree = "2222222222222222222222222222222222222222";
    c1.parents = {hRoot};
    c1.author = {"Dev", "dev@test.com", 2000, "+0000"};
    c1.committer = c1.author;
    c1.message = "Commit 1\n";
    std::string h1 = writeCommit(store, c1);

    // Commit 2 (child of commit 1)
    Commit c2;
    c2.tree = "3333333333333333333333333333333333333333";
    c2.parents = {h1};
    c2.author = {"Dev", "dev@test.com", 3000, "+0000"};
    c2.committer = c2.author;
    c2.message = "Commit 2\n";
    std::string h2 = writeCommit(store, c2);

    // Commit 3 (forked from commit 1)
    Commit c3;
    c3.tree = "4444444444444444444444444444444444444444";
    c3.parents = {h1};
    c3.author = {"Dev", "dev@test.com", 2500, "+0000"};
    c3.committer = c3.author;
    c3.message = "Commit 3 (feature)\n";
    std::string h3 = writeCommit(store, c3);

    // Test Ancestry
    TEST_ASSERT(graph.isAncestor(hRoot, h2));
    TEST_ASSERT(graph.isAncestor(h1, h2));
    TEST_ASSERT(graph.isAncestor(h1, h3));
    TEST_ASSERT(!graph.isAncestor(h2, h3));
    TEST_ASSERT(!graph.isAncestor(h3, h2));

    // Test Merge-Base (LCA)
    auto lca = graph.findMergeBase(h2, h3);
    TEST_ASSERT(lca.has_value());
    TEST_ASSERT(*lca == h1);  // Common ancestor is commit 1!

    // Merge-base where one is ancestor of the other
    auto lcaDirect = graph.findMergeBase(hRoot, h2);
    TEST_ASSERT(lcaDirect.has_value() && *lcaDirect == hRoot);

    // Test Commit History ordering (newest first)
    auto history = graph.getCommitHistory(h2);
    TEST_ASSERT(history.size() == 3);
    TEST_ASSERT(history[0] == h2);
    TEST_ASSERT(history[1] == h1);
    TEST_ASSERT(history[2] == hRoot);

    std::filesystem::remove_all(testRepo, ec);
    std::cout << "[PASS] testCommitGraphAndMergeBase\n";
}

void testBranchAndLogCommands() {
    std::cout << "[RUN] testBranchAndLogCommands\n";

    std::filesystem::path repoRoot = std::filesystem::temp_directory_path() / "test_repo_history";
    std::error_code ec;
    std::filesystem::remove_all(repoRoot, ec);
    std::filesystem::create_directories(repoRoot, ec);

    TEST_ASSERT(cmdInit({repoRoot.string()}) == 0);
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);

    // 1. Log on empty repo prints message without error
    TEST_ASSERT(cmdLog({}) == 0);

    // 2. Commit 1
    writeTestFile(repoRoot / "hello.txt", "v1\n");
    TEST_ASSERT(cmdAdd({"hello.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "First commit"}) == 0);

    // 3. Create branch 'feature'
    TEST_ASSERT(cmdBranch({"feature"}) == 0);

    // Reject duplicate branch
    TEST_ASSERT(cmdBranch({"feature"}) != 0);

    // Reject invalid branch name
    TEST_ASSERT(cmdBranch({"bad..name"}) != 0);

    // List branches
    TEST_ASSERT(cmdBranch({}) == 0);

    // 4. Commit 2 on main
    writeTestFile(repoRoot / "hello.txt", "v2\n");
    TEST_ASSERT(cmdAdd({"hello.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Second commit"}) == 0);

    // Log shows commits in order
    TEST_ASSERT(cmdLog({}) == 0);

    // 5. Delete branch 'feature'
    TEST_ASSERT(cmdBranch({"-d", "feature"}) == 0);
    TEST_ASSERT(!refs::readBranch(repoRoot / ".git", "feature").has_value());

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(repoRoot, ec);
    std::cout << "[PASS] testBranchAndLogCommands\n";
}

void testMergeAndDiffCommands() {
    std::cout << "[RUN] testMergeAndDiffCommands\n";

    std::filesystem::path repoRoot = std::filesystem::temp_directory_path() / "test_repo_merge";
    std::error_code ec;
    std::filesystem::remove_all(repoRoot, ec);
    std::filesystem::create_directories(repoRoot, ec);

    TEST_ASSERT(cmdInit({repoRoot.string()}) == 0);
    auto originalCwd = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);

    // 1. Initial commit on main
    writeTestFile(repoRoot / "file1.txt", "Original content\n");
    TEST_ASSERT(cmdAdd({"file1.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Base commit"}) == 0);

    auto baseCommit = refs::resolveHead(repoRoot / ".git");
    TEST_ASSERT(baseCommit.has_value());

    // 2. Create feature branch and switch to it
    TEST_ASSERT(cmdBranch({"feature"}) == 0);
    TEST_ASSERT(cmdCheckout({"feature"}) == 0);

    // 3. Add commits on feature branch
    writeTestFile(repoRoot / "file1.txt", "Updated content on feature\n");
    writeTestFile(repoRoot / "file2.txt", "New feature file\n");
    TEST_ASSERT(cmdAdd({"file1.txt", "file2.txt"}) == 0);
    TEST_ASSERT(cmdCommit({"-m", "Feature updates"}) == 0);

    auto featureCommit = refs::resolveHead(repoRoot / ".git");
    TEST_ASSERT(featureCommit.has_value());

    // 4. Test merge-base between main and feature
    TEST_ASSERT(cmdMergeBase({"main", "feature"}) == 0);

    // 5. Switch back to main
    TEST_ASSERT(cmdCheckout({"main"}) == 0);
    TEST_ASSERT(readTestFile(repoRoot / "file1.txt") == "Original content\n");
    TEST_ASSERT(!std::filesystem::exists(repoRoot / "file2.txt"));

    // 6. Test diff between main and feature
    TEST_ASSERT(cmdDiff({"main", "feature"}) == 0);

    // 7. Fast-forward merge feature into main!
    TEST_ASSERT(cmdMerge({"feature"}) == 0);

    // Verify main now has feature files!
    TEST_ASSERT(refs::resolveHead(repoRoot / ".git").value() == *featureCommit);
    TEST_ASSERT(readTestFile(repoRoot / "file1.txt") == "Updated content on feature\n");
    TEST_ASSERT(std::filesystem::exists(repoRoot / "file2.txt"));

    // 8. Re-merging already merged branch reports up to date
    TEST_ASSERT(cmdMerge({"feature"}) == 0);

    std::filesystem::current_path(originalCwd);
    std::filesystem::remove_all(repoRoot, ec);
    std::cout << "[PASS] testMergeAndDiffCommands\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running Teammate C (History & Diff) Tests\n";
    std::cout << "========================================\n";

    testDiffLcs();
    testCommitGraphAndMergeBase();
    testBranchAndLogCommands();
    testMergeAndDiffCommands();

    std::cout << "========================================\n";
    std::cout << "ALL TEAMMATE C TESTS PASSED SUCCESSFULLY\n";
    std::cout << "========================================\n";
    return 0;
}
