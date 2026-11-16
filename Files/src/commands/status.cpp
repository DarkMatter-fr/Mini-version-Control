#include "commands.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"
#include "sha1.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::string hashFileBlob(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return "";
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string header = "blob " + std::to_string(content.size()) + '\0';
    return sha1Hex(header + content);
}

}  // namespace

int cmdStatus(const std::vector<std::string>& /*args*/) {
    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    std::filesystem::path workTree = repoDir.parent_path();
    ObjectStore store(repoDir);

    // 1. Current branch & HEAD
    auto branch = refs::currentBranch(repoDir);
    auto headCommitHash = refs::resolveHead(repoDir);

    if (branch) {
        std::cout << "On branch " << *branch << "\n";
    } else if (headCommitHash) {
        std::cout << "HEAD detached at " << headCommitHash->substr(0, 7) << "\n";
    } else {
        std::cout << "On branch main\n";
    }

    if (!headCommitHash) {
        std::cout << "\nNo commits yet\n";
    }

    // 2. Load HEAD tree entries (path -> hash)
    std::map<std::string, std::string> headFiles;
    if (headCommitHash) {
        Commit headCommit = readCommit(store, *headCommitHash);
        auto flat = flattenTree(store, headCommit.tree);
        for (const auto& entry : flat) {
            headFiles[entry.path] = entry.hash;
        }
    }

    // 3. Load Index entries (path -> hash)
    Index index;
    index.load(repoDir / "index");
    const auto& indexFiles = index.entries();

    // 4. Staged changes: Index vs HEAD
    std::vector<std::string> stagedNew;
    std::vector<std::string> stagedModified;
    std::vector<std::string> stagedDeleted;

    for (const auto& [path, hash] : indexFiles) {
        auto it = headFiles.find(path);
        if (it == headFiles.end()) {
            stagedNew.push_back(path);
        } else if (it->second != hash) {
            stagedModified.push_back(path);
        }
    }
    for (const auto& [path, hash] : headFiles) {
        if (indexFiles.find(path) == indexFiles.end()) {
            stagedDeleted.push_back(path);
        }
    }

    // 5. Unstaged changes & Untracked files: Working tree vs Index
    std::vector<std::string> unstagedModified;
    std::vector<std::string> unstagedDeleted;
    std::vector<std::string> untrackedFiles;

    // Scan disk for untracked files and working tree state
    std::set<std::string> diskFiles;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(workTree, ec)) {
        std::string generic = entry.path().generic_string();
        if (generic.find("/" + std::string(kRepoDirName) + "/") != std::string::npos ||
            generic.rfind("/" + std::string(kRepoDirName)) == generic.size() - std::string(kRepoDirName).size() - 1 ||
            generic.find(std::string(kRepoDirName) + "/") == 0) {
            continue;
        }
        if (entry.is_regular_file(ec)) {
            std::string relPath = std::filesystem::relative(entry.path(), workTree).generic_string();
            diskFiles.insert(relPath);
        }
    }

    for (const auto& [path, indexHash] : indexFiles) {
        std::filesystem::path fullPath = workTree / path;
        if (!std::filesystem::exists(fullPath, ec)) {
            unstagedDeleted.push_back(path);
        } else {
            std::string workHash = hashFileBlob(fullPath);
            if (workHash != indexHash) {
                unstagedModified.push_back(path);
            }
        }
    }

    for (const auto& path : diskFiles) {
        if (!index.contains(path)) {
            untrackedFiles.push_back(path);
        }
    }

    // 6. Print Report
    bool hasStaged = !stagedNew.empty() || !stagedModified.empty() || !stagedDeleted.empty();
    bool hasUnstaged = !unstagedModified.empty() || !unstagedDeleted.empty();
    bool hasUntracked = !untrackedFiles.empty();

    if (hasStaged) {
        std::cout << "\nChanges to be committed:\n"
                  << "  (use \"git restore --staged <file>...\" to unstage)\n";
        for (const auto& p : stagedNew) {
            std::cout << "\tnew file:   " << p << "\n";
        }
        for (const auto& p : stagedModified) {
            std::cout << "\tmodified:   " << p << "\n";
        }
        for (const auto& p : stagedDeleted) {
            std::cout << "\tdeleted:    " << p << "\n";
        }
    }

    if (hasUnstaged) {
        std::cout << "\nChanges not staged for commit:\n"
                  << "  (use \"git add <file>...\" to update what will be committed)\n";
        for (const auto& p : unstagedModified) {
            std::cout << "\tmodified:   " << p << "\n";
        }
        for (const auto& p : unstagedDeleted) {
            std::cout << "\tdeleted:    " << p << "\n";
        }
    }

    if (hasUntracked) {
        std::cout << "\nUntracked files:\n"
                  << "  (use \"git add <file>...\" to include in what will be committed)\n";
        for (const auto& p : untrackedFiles) {
            std::cout << "\t" << p << "\n";
        }
    }

    if (!hasStaged && !hasUnstaged && !hasUntracked) {
        std::cout << "nothing to commit, working tree clean\n";
    }

    return 0;
}
