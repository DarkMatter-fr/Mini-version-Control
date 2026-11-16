#include "commands.hpp"
#include "diff.hpp"
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

std::string readFileContent(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int cmdDiff(const std::vector<std::string>& args) {
    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    std::filesystem::path workTree = repoDir.parent_path();
    ObjectStore store(repoDir);

    // Case 1: git diff <commitA> <commitB>
    if (args.size() >= 2) {
        auto hashA = resolveCommit(repoDir, store, args[0]);
        auto hashB = resolveCommit(repoDir, store, args[1]);
        if (!hashA) {
            std::cerr << "fatal: ambiguous argument '" << args[0] << "': unknown revision\n";
            return 1;
        }
        if (!hashB) {
            std::cerr << "fatal: ambiguous argument '" << args[1] << "': unknown revision\n";
            return 1;
        }

        Commit cA = readCommit(store, *hashA);
        Commit cB = readCommit(store, *hashB);

        auto flatA = flattenTree(store, cA.tree);
        auto flatB = flattenTree(store, cB.tree);

        std::map<std::string, std::string> mapA, mapB;
        std::set<std::string> allPaths;

        for (const auto& e : flatA) {
            mapA[e.path] = e.hash;
            allPaths.insert(e.path);
        }
        for (const auto& e : flatB) {
            mapB[e.path] = e.hash;
            allPaths.insert(e.path);
        }

        for (const auto& path : allPaths) {
            std::string textA = (mapA.find(path) != mapA.end()) ? readBlob(store, mapA[path]) : "";
            std::string textB = (mapB.find(path) != mapB.end()) ? readBlob(store, mapB[path]) : "";

            if (textA != textB) {
                std::cout << formatUnifiedDiff("a/" + path, "b/" + path, textA, textB);
            }
        }
        return 0;
    }

    // Case 2: git diff (working tree vs index)
    Index index;
    index.load(repoDir / "index");
    std::error_code ec;

    for (const auto& [path, indexHash] : index.entries()) {
        std::filesystem::path fullPath = workTree / path;
        std::string indexContent = readBlob(store, indexHash);

        if (!std::filesystem::exists(fullPath, ec)) {
            // Deleted file
            std::cout << formatUnifiedDiff("a/" + path, "b/" + path, indexContent, "");
        } else {
            std::string workContent = readFileContent(fullPath);
            if (workContent != indexContent) {
                std::cout << formatUnifiedDiff("a/" + path, "b/" + path, indexContent, workContent);
            }
        }
    }

    return 0;
}
