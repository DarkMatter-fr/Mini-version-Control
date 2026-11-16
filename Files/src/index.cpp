#include "index.hpp"
#include "sha1.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace {

std::string normalizePath(const std::string& p) {
    std::string out;
    out.reserve(p.size());
    for (char c : p) {
        if (c == '\\') {
            out.push_back('/');
        } else {
            out.push_back(c);
        }
    }
    // Remove leading "./"
    while (out.rfind("./", 0) == 0) {
        out = out.substr(2);
    }
    return out;
}

}  // namespace

bool Index::load(const std::filesystem::path& indexPath) {
    entries_.clear();
    std::error_code ec;
    if (!std::filesystem::exists(indexPath, ec)) {
        return true;  // Fresh empty index
    }

    std::ifstream in(indexPath, std::ios::binary);
    if (!in) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        // Expected format: <40-char-hash> <path>
        if (line.size() < 42 || line[40] != ' ') {
            continue;  // Skip malformed line
        }

        std::string hash = line.substr(0, 40);
        if (!isValidHash(hash)) {
            continue;
        }

        std::string path = normalizePath(line.substr(41));
        if (!path.empty()) {
            entries_[path] = hash;
        }
    }

    return true;
}

void Index::save(const std::filesystem::path& indexPath) const {
    std::error_code ec;
    std::filesystem::create_directories(indexPath.parent_path(), ec);

    std::ofstream out(indexPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open index file for writing: " + indexPath.string());
    }

    // std::map is automatically sorted by key (path)
    for (const auto& [path, hash] : entries_) {
        out << hash << " " << path << "\n";
    }
}

void Index::add(const std::string& path, const std::string& hash) {
    std::string norm = normalizePath(path);
    if (!norm.empty()) {
        entries_[norm] = hash;
    }
}

bool Index::remove(const std::string& path) {
    return entries_.erase(normalizePath(path)) > 0;
}

std::optional<std::string> Index::getHash(const std::string& path) const {
    auto it = entries_.find(normalizePath(path));
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Index::contains(const std::string& path) const {
    return entries_.find(normalizePath(path)) != entries_.end();
}

void Index::clear() {
    entries_.clear();
}

size_t Index::size() const {
    return entries_.size();
}

bool Index::empty() const {
    return entries_.empty();
}

const std::map<std::string, std::string>& Index::entries() const {
    return entries_;
}

