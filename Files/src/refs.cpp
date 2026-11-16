#include "refs.hpp"
#include "sha1.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace refs {

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\r' || s[start] == '\n' || s[start] == '\t')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\r' || s[end - 1] == '\n' || s[end - 1] == '\t')) {
        --end;
    }
    return s.substr(start, end - start);
}

}  // namespace

std::optional<std::string> readHead(const std::filesystem::path& repoDir) {
    std::filesystem::path headPath = repoDir / "HEAD";
    std::error_code ec;
    if (!std::filesystem::exists(headPath, ec)) {
        return std::nullopt;
    }
    std::ifstream in(headPath);
    if (!in) {
        return std::nullopt;
    }
    std::string line;
    std::getline(in, line);
    return trim(line);
}

bool isHeadSymbolic(const std::filesystem::path& repoDir) {
    auto head = readHead(repoDir);
    return head.has_value() && head->rfind("ref: ", 0) == 0;
}

std::optional<std::string> currentBranch(const std::filesystem::path& repoDir) {
    auto head = readHead(repoDir);
    if (!head || head->rfind("ref: refs/heads/", 0) != 0) {
        return std::nullopt;
    }
    return head->substr(std::string("ref: refs/heads/").size());
}

std::optional<std::string> resolveHead(const std::filesystem::path& repoDir) {
    auto head = readHead(repoDir);
    if (!head) {
        return std::nullopt;
    }

    if (head->rfind("ref: refs/heads/", 0) == 0) {
        std::string branch = head->substr(std::string("ref: refs/heads/").size());
        return readBranch(repoDir, branch);
    }

    if (isValidHash(*head)) {
        return *head;
    }

    return std::nullopt;
}

std::optional<std::string> readBranch(const std::filesystem::path& repoDir, const std::string& branchName) {
    std::filesystem::path branchPath = repoDir / "refs" / "heads" / branchName;
    std::error_code ec;
    if (!std::filesystem::exists(branchPath, ec)) {
        return std::nullopt;
    }
    std::ifstream in(branchPath);
    if (!in) {
        return std::nullopt;
    }
    std::string line;
    std::getline(in, line);
    std::string hash = trim(line);
    if (isValidHash(hash)) {
        return hash;
    }
    return std::nullopt;
}

void writeBranch(const std::filesystem::path& repoDir, const std::string& branchName, const std::string& commitHash) {
    std::filesystem::path branchPath = repoDir / "refs" / "heads" / branchName;
    std::error_code ec;
    std::filesystem::create_directories(branchPath.parent_path(), ec);

    std::ofstream out(branchPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to write branch ref: " + branchPath.string());
    }
    out << commitHash << "\n";
}

void setHeadToBranch(const std::filesystem::path& repoDir, const std::string& branchName) {
    std::filesystem::path headPath = repoDir / "HEAD";
    std::ofstream out(headPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to update HEAD: " + headPath.string());
    }
    out << "ref: refs/heads/" << branchName << "\n";
}

void setHeadDetached(const std::filesystem::path& repoDir, const std::string& commitHash) {
    std::filesystem::path headPath = repoDir / "HEAD";
    std::ofstream out(headPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to update HEAD: " + headPath.string());
    }
    out << commitHash << "\n";
}

std::vector<std::string> listBranches(const std::filesystem::path& repoDir) {
    std::vector<std::string> branches;
    std::filesystem::path headsDir = repoDir / "refs" / "heads";
    std::error_code ec;
    if (!std::filesystem::exists(headsDir, ec)) {
        return branches;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(headsDir, ec)) {
        if (entry.is_regular_file()) {
            std::string branch = std::filesystem::relative(entry.path(), headsDir).generic_string();
            branches.push_back(branch);
        }
    }
    std::sort(branches.begin(), branches.end());
    return branches;
}

bool isValidBranchName(const std::string& name) {
    if (name.empty()) return false;
    if (name.front() == '.' || name.back() == '.' || name.front() == '/' || name.back() == '/') {
        return false;
    }
    if (name.find("..") != std::string::npos) return false;
    for (char c : name) {
        if (static_cast<unsigned char>(c) < 32 || c == 127) return false;
        if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\') {
            return false;
        }
    }
    return true;
}

}  // namespace refs
