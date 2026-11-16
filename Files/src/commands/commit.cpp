#include "commands.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int cmdCommit(const std::vector<std::string>& args) {
    std::string message;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m") {
            if (i + 1 < args.size()) {
                message = args[++i];
            } else {
                std::cerr << "error: switch 'm' requires a value\n";
                return 1;
            }
        }
    }

    if (message.empty()) {
        std::cerr << "error: empty commit message\n";
        return 1;
    }
    if (message.back() != '\n') {
        message.push_back('\n');
    }

    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    Index index;
    index.load(repoDir / "index");

    ObjectStore store(repoDir);
    auto headCommitHash = refs::resolveHead(repoDir);

    if (index.empty()) {
        if (!headCommitHash) {
            std::cout << "nothing to commit (create/copy files and use \"git add\" to track)\n";
        } else {
            std::cout << "nothing to commit, working tree clean\n";
        }
        return 1;
    }

    // 1. Build Merkle tree from index entries (bottom-up post-order)
    TreeNode root;
    root.mode = kModeDir;
    for (const auto& [path, hash] : index.entries()) {
        insertPath(root, path, hash, kModeFile);
    }
    std::string newTreeHash = writeTree(store, root);

    // 2. Check if tree matches parent commit tree
    if (headCommitHash) {
        Commit headCommit = readCommit(store, *headCommitHash);
        if (headCommit.tree == newTreeHash) {
            std::cout << "nothing to commit, working tree clean\n";
            return 1;
        }
    }

    // 3. Assemble commit object
    const char* envName = std::getenv("GIT_AUTHOR_NAME");
    if (!envName) envName = std::getenv("USERNAME");
    if (!envName) envName = "User";

    const char* envEmail = std::getenv("GIT_AUTHOR_EMAIL");
    if (!envEmail) envEmail = "user@example.com";

    long long now = static_cast<long long>(std::time(nullptr));

    Commit c;
    c.tree = newTreeHash;
    if (headCommitHash) {
        c.parents.push_back(*headCommitHash);
    }
    c.author.name = envName;
    c.author.email = envEmail;
    c.author.timestamp = now;
    c.author.tz = "+0000";
    c.committer = c.author;
    c.message = message;

    std::string newCommitHash = writeCommit(store, c);

    // 4. Update branch ref or detached HEAD
    auto branch = refs::currentBranch(repoDir);
    if (branch) {
        refs::writeBranch(repoDir, *branch, newCommitHash);
    } else {
        refs::setHeadDetached(repoDir, newCommitHash);
    }

    // 5. Output summary
    std::string branchName = branch ? *branch : "detached HEAD";
    bool isRoot = !headCommitHash.has_value();
    std::string firstLine = message.substr(0, message.find('\n'));
    std::cout << "[" << branchName << (isRoot ? " (root-commit) " : " ")
              << newCommitHash.substr(0, 7) << "] " << firstLine << "\n";
    std::cout << " " << index.size() << " file(s) changed\n";
    return 0;
}

