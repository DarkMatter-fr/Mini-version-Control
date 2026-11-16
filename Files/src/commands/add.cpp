#include "commands.hpp"
#include "index.hpp"
#include "objects.hpp"
#include "object_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int cmdAdd(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "fatal: No path specified. Usage: git add <path>...\n";
        return 1;
    }

    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    std::filesystem::path workTree = repoDir.parent_path();

    // 1. Validate all path arguments first before modifying the index or writing objects
    std::vector<std::filesystem::path> validatedPaths;
    for (const auto& arg : args) {
        std::filesystem::path p = std::filesystem::absolute(arg);
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) {
            std::cerr << "fatal: pathspec '" << arg << "' did not match any files\n";
            return 1;
        }
        validatedPaths.push_back(p);
    }

    // 2. Collect all files to add
    std::vector<std::filesystem::path> filesToAdd;
    for (const auto& p : validatedPaths) {
        std::error_code ec;
        if (std::filesystem::is_directory(p, ec)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(p, ec)) {
                // Skip anything inside .git
                std::string generic = entry.path().generic_string();
                if (generic.find("/" + std::string(kRepoDirName) + "/") != std::string::npos ||
                    generic.rfind("/" + std::string(kRepoDirName)) == generic.size() - std::string(kRepoDirName).size() - 1 ||
                    generic.find(std::string(kRepoDirName) + "/") == 0) {
                    continue;
                }
                if (entry.is_regular_file(ec)) {
                    filesToAdd.push_back(entry.path());
                }
            }
        } else if (std::filesystem::is_regular_file(p, ec)) {
            filesToAdd.push_back(p);
        }
    }

    // 3. Load current index
    Index index;
    std::filesystem::path indexPath = repoDir / "index";
    index.load(indexPath);

    // 4. Store blobs and update index
    ObjectStore store(repoDir);
    for (const auto& file : filesToAdd) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            std::cerr << "fatal: could not open file '" << file.string() << "'\n";
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        std::string hash = writeBlob(store, content);
        std::string relPath = std::filesystem::relative(file, workTree).generic_string();
        index.add(relPath, hash);
    }

    // 5. Save updated index
    index.save(indexPath);
    return 0;
}

