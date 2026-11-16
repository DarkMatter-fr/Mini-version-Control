#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace refs {

// Reads HEAD file from repoDir. Returns raw content trimmed (e.g. "ref: refs/heads/main" or 40-char hash).
std::optional<std::string> readHead(const std::filesystem::path& repoDir);

// Returns true if HEAD is a symbolic ref pointing to a branch
bool isHeadSymbolic(const std::filesystem::path& repoDir);

// Returns the branch name HEAD points to (e.g. "main"), or nullopt if detached HEAD or corrupt
std::optional<std::string> currentBranch(const std::filesystem::path& repoDir);

// Resolves HEAD to the current commit hash (if any). Returns nullopt if empty repo (unborn branch).
std::optional<std::string> resolveHead(const std::filesystem::path& repoDir);

// Reads the commit hash of a branch (e.g. "main"). Returns nullopt if branch does not exist.
std::optional<std::string> readBranch(const std::filesystem::path& repoDir, const std::string& branchName);

// Writes a commit hash to a branch (e.g. "main"). Creates .git/refs/heads/ if needed.
void writeBranch(const std::filesystem::path& repoDir, const std::string& branchName, const std::string& commitHash);

// Sets HEAD to a branch ("ref: refs/heads/<branchName>\n")
void setHeadToBranch(const std::filesystem::path& repoDir, const std::string& branchName);

// Sets HEAD to a detached commit hash ("<commitHash>\n")
void setHeadDetached(const std::filesystem::path& repoDir, const std::string& commitHash);

// Lists all local branch names
std::vector<std::string> listBranches(const std::filesystem::path& repoDir);

// Checks if a branch name is valid
bool isValidBranchName(const std::string& name);

}  // namespace refs
