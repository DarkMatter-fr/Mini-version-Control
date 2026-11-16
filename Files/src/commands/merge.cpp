#include "commands.hpp"
#include "commit_graph.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"
#include "sha1.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

std::optional<std::string> resolveCommit(const std::filesystem::path& repoDir, ObjectStore& store, const std::string& name) {
    auto bHash = refs::readBranch(repoDir, name);
    if (bHash) return *bHash;

    if (name == "HEAD") {
        return refs::resolveHead(repoDir);
    }

    if (isValidHash(name)) {
        if (store.exists(name)) return name;
    }

    if (name.size() >= 4 && name.size() < 40) {
        std::filesystem::path dir = repoDir / "objects" / name.substr(0, 2);
        std::string prefix = name.substr(2);
        std::error_code ec;
        std::vector<std::string> matches;
        if (std::filesystem::exists(dir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                std::string fname = entry.path().filename().string();
                if (fname.rfind(prefix, 0) == 0) {
                    matches.push_back(name.substr(0, 2) + fname);
                }
            }
        }
        if (matches.size() == 1) return matches[0];
    }

    return std::nullopt;
}

void removeEmptyParentDirs(const std::filesystem::path& startDir, const std::filesystem::path& stopDir) {
    std::error_code ec;
    std::filesystem::path curr = startDir;
    while (curr != stopDir && !curr.empty() && curr != curr.parent_path()) {
        if (std::filesystem::is_empty(curr, ec) && !ec) {
            std::filesystem::remove(curr, ec);
            curr = curr.parent_path();
        } else {
            break;
        }
    }
}

}  // namespace

int cmdMergeBase(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "usage: git merge-base <commit> <commit>\n";
        return 1;
    }

    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    ObjectStore store(repoDir);
    CommitGraph graph(store);

    auto hashA = resolveCommit(repoDir, store, args[0]);
    if (!hashA) {
        std::cerr << "fatal: Not a valid object name " << args[0] << "\n";
        return 1;
    }

    auto hashB = resolveCommit(repoDir, store, args[1]);
    if (!hashB) {
        std::cerr << "fatal: Not a valid object name " << args[1] << "\n";
        return 1;
    }

    auto lca = graph.findMergeBase(*hashA, *hashB);
    if (!lca) {
        return 1;
    }

    std::cout << *lca << "\n";
    return 0;
}

int cmdMerge(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "fatal: No branch specified to merge.\n";
        return 1;
    }

    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    std::filesystem::path workTree = repoDir.parent_path();
    ObjectStore store(repoDir);
    CommitGraph graph(store);

    auto currentHead = refs::resolveHead(repoDir);
    auto currentBranch = refs::currentBranch(repoDir);
    if (!currentHead) {
        std::cerr << "fatal: current branch does not have any commits.\n";
        return 1;
    }

    std::string targetBranchName = args[0];
    auto targetCommit = resolveCommit(repoDir, store, targetBranchName);
    if (!targetCommit) {
        std::cerr << "merge: " << targetBranchName << " - not something we can merge\n";
        return 1;
    }

    if (*currentHead == *targetCommit) {
        std::cout << "Already up to date.\n";
        return 0;
    }

    // Check if target is an ancestor of current HEAD (already merged)
    if (graph.isAncestor(*targetCommit, *currentHead)) {
        std::cout << "Already up to date.\n";
        return 0;
    }

    // Check for Fast-Forward: current HEAD is an ancestor of target commit
    if (graph.isAncestor(*currentHead, *targetCommit)) {
        std::cout << "Updating " << currentHead->substr(0, 7) << ".." << targetCommit->substr(0, 7) << "\n";
        std::cout << "Fast-forward\n";

        // Restore target tree into working directory and update index
        Commit tc = readCommit(store, *targetCommit);
        auto flatTarget = flattenTree(store, tc.tree);
        std::map<std::string, std::string> targetFiles;
        for (const auto& e : flatTarget) {
            targetFiles[e.path] = e.hash;
        }

        Index index;
        index.load(repoDir / "index");
        std::error_code ec;

        // Remove files absent from target
        for (const auto& [path, _] : index.entries()) {
            if (targetFiles.find(path) == targetFiles.end()) {
                std::filesystem::path fullPath = workTree / path;
                std::filesystem::remove(fullPath, ec);
                removeEmptyParentDirs(fullPath.parent_path(), workTree);
            }
        }

        // Write target files
        for (const auto& [path, hash] : targetFiles) {
            std::filesystem::path fullPath = workTree / path;
            std::filesystem::create_directories(fullPath.parent_path(), ec);

            std::string content = readBlob(store, hash);
            std::ofstream out(fullPath, std::ios::binary | std::ios::trunc);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        // Save index
        index.clear();
        for (const auto& [path, hash] : targetFiles) {
            index.add(path, hash);
        }
        index.save(repoDir / "index");

        // Update branch ref or detached HEAD
        if (currentBranch) {
            refs::writeBranch(repoDir, *currentBranch, *targetCommit);
        } else {
            refs::setHeadDetached(repoDir, *targetCommit);
        }

        std::cout << " " << targetFiles.size() << " file(s) updated\n";
        return 0;
    }

    std::cerr << "fatal: non-fast-forward merge not supported\n";
    return 1;
}
