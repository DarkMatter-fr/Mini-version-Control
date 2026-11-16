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
#include <optional>
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

int cmdCheckout(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "fatal: You must specify a branch or commit to checkout.\n";
        return 1;
    }

    std::string target = args[0];
    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    std::filesystem::path workTree = repoDir.parent_path();
    ObjectStore store(repoDir);

    // 1. Resolve target to branch or commit hash
    std::optional<std::string> targetBranch;
    std::string targetCommitHash;

    auto branchHash = refs::readBranch(repoDir, target);
    if (branchHash) {
        targetBranch = target;
        targetCommitHash = *branchHash;
    } else {
        // Check if target is a full or prefix commit hash
        std::string fullHash;
        if (isValidHash(target)) {
            fullHash = target;
        } else if (target.size() >= 4 && target.size() < 40) {
            // Check prefix in objects dir
            std::filesystem::path dir = repoDir / "objects" / target.substr(0, 2);
            std::string prefix = target.substr(2);
            std::error_code ec;
            std::vector<std::string> matches;
            if (std::filesystem::exists(dir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                    std::string fname = entry.path().filename().string();
                    if (fname.rfind(prefix, 0) == 0) {
                        matches.push_back(target.substr(0, 2) + fname);
                    }
                }
            }
            if (matches.size() == 1) {
                fullHash = matches[0];
            } else if (matches.size() > 1) {
                std::cerr << "error: short SHA1 " << target << " is ambiguous\n";
                return 1;
            }
        }

        if (!fullHash.empty()) {
            try {
                auto obj = store.read(fullHash);
                if (obj && obj->type == "commit") {
                    targetCommitHash = fullHash;
                }
            } catch (...) {
            }
        }
    }

    if (targetCommitHash.empty()) {
        std::cerr << "error: pathspec '" << target << "' did not match any file(s) known to git\n";
        return 1;
    }

    // 2. Load target tree
    Commit targetCommit;
    try {
        targetCommit = readCommit(store, targetCommitHash);
    } catch (const std::exception& e) {
        std::cerr << "fatal: failed to read target commit: " << e.what() << "\n";
        return 1;
    }

    auto flatTarget = flattenTree(store, targetCommit.tree);
    std::map<std::string, std::string> targetFiles;
    for (const auto& entry : flatTarget) {
        targetFiles[entry.path] = entry.hash;
    }

    // 3. Load current HEAD and Index
    auto currentHeadHash = refs::resolveHead(repoDir);
    std::map<std::string, std::string> headFiles;
    if (currentHeadHash) {
        Commit currentCommit = readCommit(store, *currentHeadHash);
        auto flatHead = flattenTree(store, currentCommit.tree);
        for (const auto& entry : flatHead) {
            headFiles[entry.path] = entry.hash;
        }
    }

    Index index;
    index.load(repoDir / "index");
    const auto& indexFiles = index.entries();

    // 4. Dirty-tree checks (prevent data loss)
    std::vector<std::string> dirtyLocalFiles;
    std::vector<std::string> untrackedOverwritten;
    std::error_code ec;

    // A. Check modified tracked files
    for (const auto& [path, indexHash] : indexFiles) {
        std::filesystem::path fullPath = workTree / path;
        bool existsOnDisk = std::filesystem::exists(fullPath, ec);
        std::string diskHash = existsOnDisk ? hashFileBlob(fullPath) : "";

        bool hasUnstagedChange = !existsOnDisk || (diskHash != indexHash);
        bool hasStagedChange = (headFiles.find(path) == headFiles.end()) || (headFiles[path] != indexHash);

        if (hasUnstagedChange || hasStagedChange) {
            // Would checkout alter this file?
            auto targetIt = targetFiles.find(path);
            if (targetIt == targetFiles.end() || targetIt->second != diskHash) {
                dirtyLocalFiles.push_back(path);
            }
        }
    }

    // B. Check untracked files that would be overwritten by target
    for (const auto& [path, targetHash] : targetFiles) {
        if (indexFiles.find(path) == indexFiles.end()) {
            std::filesystem::path fullPath = workTree / path;
            if (std::filesystem::exists(fullPath, ec)) {
                std::string diskHash = hashFileBlob(fullPath);
                if (diskHash != targetHash) {
                    untrackedOverwritten.push_back(path);
                }
            }
        }
    }

    if (!dirtyLocalFiles.empty()) {
        std::cerr << "error: Your local changes to the following files would be overwritten by checkout:\n";
        for (const auto& p : dirtyLocalFiles) {
            std::cerr << "    " << p << "\n";
        }
        std::cerr << "Please commit your changes or stash them before you switch branches.\nAborting\n";
        return 1;
    }

    if (!untrackedOverwritten.empty()) {
        std::cerr << "error: The following untracked working tree files would be overwritten by checkout:\n";
        for (const auto& p : untrackedOverwritten) {
            std::cerr << "    " << p << "\n";
        }
        std::cerr << "Please move or remove them before you switch branches.\nAborting\n";
        return 1;
    }

    // 5. Restore files from target tree
    // Remove files that were in current index but absent in target
    for (const auto& [path, _] : indexFiles) {
        if (targetFiles.find(path) == targetFiles.end()) {
            std::filesystem::path fullPath = workTree / path;
            std::filesystem::remove(fullPath, ec);
            removeEmptyParentDirs(fullPath.parent_path(), workTree);
        }
    }

    // Write / overwrite target files
    for (const auto& [path, hash] : targetFiles) {
        std::filesystem::path fullPath = workTree / path;
        std::filesystem::create_directories(fullPath.parent_path(), ec);

        std::string content = readBlob(store, hash);
        std::ofstream out(fullPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "fatal: failed to write file: " << fullPath.string() << "\n";
            return 1;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    // 6. Update index to match target tree exactly
    index.clear();
    for (const auto& [path, hash] : targetFiles) {
        index.add(path, hash);
    }
    index.save(repoDir / "index");

    // 7. Update HEAD
    if (targetBranch) {
        refs::setHeadToBranch(repoDir, *targetBranch);
        std::cout << "Switched to branch '" << *targetBranch << "'\n";
    } else {
        refs::setHeadDetached(repoDir, targetCommitHash);
        std::cout << "Note: switching to '" << targetCommitHash.substr(0, 7) << "' (detached HEAD)\n";
        std::cout << "HEAD is now at " << targetCommitHash.substr(0, 7) << " "
                  << targetCommit.message.substr(0, targetCommit.message.find('\n')) << "\n";
    }

    return 0;
}

