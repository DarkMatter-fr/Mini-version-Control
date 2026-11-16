#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>

struct IndexEntry {
    std::string path;  // relative to work tree, using '/'
    std::string hash;  // 40-character hex SHA-1
};

class Index {
public:
    Index() = default;

    // Load from index file on disk (<repoDir>/index)
    bool load(const std::filesystem::path& indexPath);

    // Save to index file on disk (<repoDir>/index), sorted by path
    void save(const std::filesystem::path& indexPath) const;

    // Add or update an entry
    void add(const std::string& path, const std::string& hash);

    // Remove an entry by path (returns true if removed)
    bool remove(const std::string& path);

    // Lookup hash by path
    std::optional<std::string> getHash(const std::string& path) const;

    bool contains(const std::string& path) const;

    void clear();

    size_t size() const;
    bool empty() const;

    // All entries sorted by path
    const std::map<std::string, std::string>& entries() const;

private:
    std::map<std::string, std::string> entries_;  // path -> hash
};

