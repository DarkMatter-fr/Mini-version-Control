#include "commands.hpp"
#include "object_store.hpp"
#include "refs.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int cmdBranch(const std::vector<std::string>& args) {
    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;

    // 1. List branches when no arguments given
    if (args.empty()) {
        auto current = refs::currentBranch(repoDir);
        auto branches = refs::listBranches(repoDir);

        if (branches.empty()) {
            std::cout << "* " << (current ? *current : "main") << "\n";
            return 0;
        }

        for (const auto& b : branches) {
            if (current && *current == b) {
                std::cout << "* " << b << "\n";
            } else {
                std::cout << "  " << b << "\n";
            }
        }
        return 0;
    }

    // 2. Handle branch deletion: git branch -d <name>
    if (args[0] == "-d" || args[0] == "-D") {
        if (args.size() < 2) {
            std::cerr << "fatal: branch name required\n";
            return 1;
        }
        std::string name = args[1];
        auto current = refs::currentBranch(repoDir);
        if (current && *current == name) {
            std::cerr << "error: Cannot delete branch '" << name << "' checked out at '" << repoDir.parent_path().string() << "'\n";
            return 1;
        }
        std::filesystem::path branchPath = repoDir / "refs" / "heads" / name;
        std::error_code ec;
        if (!std::filesystem::exists(branchPath, ec)) {
            std::cerr << "error: branch '" << name << "' not found.\n";
            return 1;
        }
        std::filesystem::remove(branchPath, ec);
        std::cout << "Deleted branch " << name << ".\n";
        return 0;
    }

    // 3. Create branch: git branch <name>
    std::string newBranch = args[0];
    if (!refs::isValidBranchName(newBranch)) {
        std::cerr << "fatal: '" << newBranch << "' is not a valid branch name\n";
        return 1;
    }

    if (refs::readBranch(repoDir, newBranch).has_value()) {
        std::cerr << "fatal: a branch named '" << newBranch << "' already exists\n";
        return 1;
    }

    auto headCommit = refs::resolveHead(repoDir);
    if (!headCommit) {
        std::cerr << "fatal: not a valid object name: 'HEAD'\n";
        return 1;
    }

    refs::writeBranch(repoDir, newBranch, *headCommit);
    return 0;
}
