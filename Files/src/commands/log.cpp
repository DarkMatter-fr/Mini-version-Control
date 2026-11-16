#include "commands.hpp"
#include "commit_graph.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "refs.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string formatCommitDate(long long timestamp, const std::string& tz) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm* tmInfo = std::gmtime(&t);
    char buf[64] = {0};
    if (tmInfo) {
        std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", tmInfo);
    }
    std::string out = buf;
    out += " " + (tz.empty() ? "+0000" : tz);
    return out;
}

}  // namespace

int cmdLog(const std::vector<std::string>& /*args*/) {
    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    std::filesystem::path repoDir = *repo;
    auto headCommitHash = refs::resolveHead(repoDir);
    if (!headCommitHash) {
        auto branch = refs::currentBranch(repoDir);
        std::cerr << "fatal: your current branch '" << (branch ? *branch : "main")
                  << "' does not have any commits yet\n";
        return 0;
    }

    ObjectStore store(repoDir);
    CommitGraph graph(store);

    auto history = graph.getCommitHistory(*headCommitHash);
    for (size_t i = 0; i < history.size(); ++i) {
        const std::string& hash = history[i];
        try {
            Commit c = readCommit(store, hash);
            std::cout << "commit " << hash << "\n";
            if (c.parents.size() > 1) {
                std::cout << "Merge:";
                for (const auto& p : c.parents) {
                    std::cout << " " << p.substr(0, 7);
                }
                std::cout << "\n";
            }
            std::cout << "Author: " << c.author.name << " <" << c.author.email << ">\n";
            std::cout << "Date:   " << formatCommitDate(c.author.timestamp, c.author.tz) << "\n\n";

            // Indent message with 4 spaces
            std::istringstream stream(c.message);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                std::cout << "    " << line << "\n";
            }
            if (i + 1 < history.size()) {
                std::cout << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "error: failed to read commit " << hash << ": " << e.what() << "\n";
        }
    }

    return 0;
}
