#include "object_store.hpp"
#include "sha1.hpp"

#include <fstream>
#include <stdexcept>
#include <system_error>

ObjectStore::ObjectStore(std::filesystem::path repoDir)
    : objectsDir_(std::move(repoDir) / "objects") {}

std::filesystem::path ObjectStore::pathFor(const std::string& hash) const {
    if (!isValidHash(hash)) {
        throw std::runtime_error("Invalid object hash: " + hash);
    }
    return objectsDir_ / hash.substr(0, 2) / hash.substr(2);
}

bool ObjectStore::exists(const std::string& hash) const {
    if (!isValidHash(hash)) {
        return false;
    }
    if (cache_.contains(hash)) {
        return true;
    }
    std::error_code ec;
    return std::filesystem::exists(pathFor(hash), ec);
}

std::string ObjectStore::write(const std::string& type, const std::string& content) {
    std::string header = type + " " + std::to_string(content.size()) + '\0';
    std::string raw = header + content;
    std::string hash = sha1Hex(raw);

    // If already in memory cache, nothing more to do
    if (cache_.contains(hash)) {
        return hash;
    }

    std::filesystem::path p = pathFor(hash);
    std::error_code ec;
    if (std::filesystem::exists(p, ec)) {
        // Idempotent: already exists on disk
        cache_.insert(hash, Object{type, content});
        return hash;
    }

    // Ensure parent directory exists: objects/ab/
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
        throw std::runtime_error("Failed to create object directory: " + p.parent_path().string());
    }

    // Write file in binary mode
    std::ofstream out(p, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open object file for writing: " + p.string());
    }
    out.write(raw.data(), static_cast<std::streamsize>(raw.size()));
    if (!out) {
        throw std::runtime_error("Failed to write object contents to: " + p.string());
    }
    out.close();

    cache_.insert(hash, Object{type, content});
    return hash;
}

std::optional<Object> ObjectStore::read(const std::string& hash) {
    if (!isValidHash(hash)) {
        throw std::runtime_error("Invalid object hash: " + hash);
    }

    if (const Object* cached = cache_.find(hash)) {
        return *cached;
    }

    std::filesystem::path p = pathFor(hash);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) {
        return std::nullopt;
    }

    std::ifstream in(p, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open object file for reading: " + p.string());
    }
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // Verify hash integrity
    if (sha1Hex(raw) != hash) {
        throw std::runtime_error("Corrupt object: hash mismatch for " + hash);
    }

    // Parse header: "<type> <size>\0<content>"
    size_t spacePos = raw.find(' ');
    if (spacePos == std::string::npos) {
        throw std::runtime_error("Corrupt object: missing space in header for " + hash);
    }
    std::string type = raw.substr(0, spacePos);

    size_t nullPos = raw.find('\0', spacePos + 1);
    if (nullPos == std::string::npos) {
        throw std::runtime_error("Corrupt object: missing null byte in header for " + hash);
    }
    std::string sizeStr = raw.substr(spacePos + 1, nullPos - spacePos - 1);
    size_t size = 0;
    try {
        size = std::stoull(sizeStr);
    } catch (...) {
        throw std::runtime_error("Corrupt object: invalid size in header for " + hash);
    }

    std::string content = raw.substr(nullPos + 1);
    if (content.size() != size) {
        throw std::runtime_error("Corrupt object: content size mismatch for " + hash);
    }

    Object obj{std::move(type), std::move(content)};
    cache_.insert(hash, obj);
    return obj;
}

std::optional<std::filesystem::path> findRepoDir(std::filesystem::path start) {
    std::error_code ec;
    std::filesystem::path curr = std::filesystem::absolute(start, ec);
    if (ec) {
        curr = start;
    }

    while (true) {
        std::filesystem::path candidate = curr / kRepoDirName;
        if (std::filesystem::is_directory(candidate, ec)) {
            return candidate;
        }
        std::filesystem::path parent = curr.parent_path();
        if (parent == curr || curr.empty()) {
            break;
        }
        curr = parent;
    }
    return std::nullopt;
}

