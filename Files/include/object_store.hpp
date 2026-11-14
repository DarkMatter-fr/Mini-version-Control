#pragma once
// Content-addressable object database.
//   write(type, content) -> hash     (idempotent: same content, same hash, one file)
//   read(hash)           -> Object   (validates header, size and hash)
// On disk:  <repoDir>/objects/ab/cdef...   (first 2 hex chars = directory)
// Stored bytes: "<type> <size>\0<content>"  (same framing as Git, so hashes match).
#include <filesystem>
#include <optional>
#include <string>

#include "hash_table.hpp"

struct Object {
    std::string type;     // "blob" | "tree" | "commit"
    std::string content;  // raw payload without the header
};

class ObjectStore {
public:
    // repoDir is the repository metadata directory (e.g. ".git").
    explicit ObjectStore(std::filesystem::path repoDir);

    std::string write(const std::string& type, const std::string& content);

    // nullopt  -> object does not exist.
    // throws std::runtime_error -> object exists but is corrupt, or hash is malformed.
    std::optional<Object> read(const std::string& hash);

    bool exists(const std::string& hash) const;

private:
    std::filesystem::path pathFor(const std::string& hash) const;

    std::filesystem::path objectsDir_;
    HashTable<std::string, Object> cache_;  // in-memory cache of objects already read/written
};

// Name of the metadata directory, and a helper to locate it by walking up from `start`.
inline constexpr const char* kRepoDirName = ".git";
std::optional<std::filesystem::path> findRepoDir(std::filesystem::path start = std::filesystem::current_path());
