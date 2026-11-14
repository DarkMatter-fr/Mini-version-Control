#include "commands.hpp"
#include "object_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

int cmdInit(const std::vector<std::string>& args) {
    std::filesystem::path target = std::filesystem::current_path();
    if (!args.empty()) {
        target = std::filesystem::path(args[0]);
    }

    std::filesystem::path repoDir = target / kRepoDirName;
    std::error_code ec;
    if (std::filesystem::exists(repoDir, ec)) {
        std::cerr << "fatal: repository already exists at " << repoDir.string() << "\n";
        return 1;
    }

    std::filesystem::path objectsDir = repoDir / "objects";
    std::filesystem::path refsHeadsDir = repoDir / "refs" / "heads";
    std::filesystem::path headFile = repoDir / "HEAD";

    if (!std::filesystem::create_directories(objectsDir, ec) && ec) {
        std::cerr << "fatal: failed to create directory " << objectsDir.string() << ": " << ec.message() << "\n";
        return 1;
    }
    if (!std::filesystem::create_directories(refsHeadsDir, ec) && ec) {
        std::cerr << "fatal: failed to create directory " << refsHeadsDir.string() << ": " << ec.message() << "\n";
        return 1;
    }

    std::ofstream head(headFile, std::ios::binary);
    if (!head) {
        std::cerr << "fatal: failed to create HEAD file at " << headFile.string() << "\n";
        return 1;
    }
    head << "ref: refs/heads/main\n";
    head.close();

    std::filesystem::path canonicalRepo = std::filesystem::weakly_canonical(repoDir, ec);
    if (ec) {
        canonicalRepo = repoDir;
    }
    std::cout << "Initialized empty Git repository in " << canonicalRepo.string() << "\n";
    return 0;
}

