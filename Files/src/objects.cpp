#include "objects.hpp"
#include "sha1.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------- blob

std::string writeBlob(ObjectStore& store, const std::string& content) {
    return store.write("blob", content);
}

std::string readBlob(ObjectStore& store, const std::string& hash) {
    auto opt = store.read(hash);
    if (!opt) {
        throw std::runtime_error("Object not found: " + hash);
    }
    if (opt->type != "blob") {
        throw std::runtime_error("Object " + hash + " is not a blob (is " + opt->type + ")");
    }
    return opt->content;
}

// ---------------------------------------------------------------- tree

std::string serializeTree(std::vector<TreeEntry> entries) {
    // In Git, trees sort entries by name, but directory entries compare as if
    // they had a trailing slash '/'.
    auto sortKey = [](const TreeEntry& e) {
        return e.isDir() ? e.name + "/" : e.name;
    };
    std::sort(entries.begin(), entries.end(), [&](const TreeEntry& a, const TreeEntry& b) {
        return sortKey(a) < sortKey(b);
    });

    std::string out;
    for (const auto& entry : entries) {
        out += entry.mode;
        out.push_back(' ');
        out += entry.name;
        out.push_back('\0');
        out += fromHex(entry.hash);
    }
    return out;
}

std::vector<TreeEntry> parseTree(const std::string& content) {
    std::vector<TreeEntry> entries;
    size_t i = 0;
    while (i < content.size()) {
        size_t spacePos = content.find(' ', i);
        if (spacePos == std::string::npos) {
            throw std::runtime_error("Corrupt tree object: missing space after mode");
        }
        std::string mode = content.substr(i, spacePos - i);
        if (mode == "040000") {
            mode = kModeDir;
        }

        size_t nullPos = content.find('\0', spacePos + 1);
        if (nullPos == std::string::npos) {
            throw std::runtime_error("Corrupt tree object: missing null byte after name");
        }
        std::string name = content.substr(spacePos + 1, nullPos - spacePos - 1);

        if (nullPos + 1 + 20 > content.size()) {
            throw std::runtime_error("Corrupt tree object: truncated SHA-1 hash");
        }
        std::string rawHash = content.substr(nullPos + 1, 20);
        std::string hexHash = toHex(rawHash);

        entries.push_back(TreeEntry{std::move(mode), std::move(name), std::move(hexHash)});
        i = nullPos + 1 + 20;
    }
    return entries;
}

std::string writeTreeObject(ObjectStore& store, const std::vector<TreeEntry>& entries) {
    return store.write("tree", serializeTree(entries));
}

std::vector<TreeEntry> readTreeObject(ObjectStore& store, const std::string& hash) {
    auto opt = store.read(hash);
    if (!opt) {
        throw std::runtime_error("Object not found: " + hash);
    }
    if (opt->type != "tree") {
        throw std::runtime_error("Object " + hash + " is not a tree (is " + opt->type + ")");
    }
    return parseTree(opt->content);
}

// ---------------------------------------------------------------- N-ary tree node

TreeNode* TreeNode::findChild(const std::string& childName) {
    for (const auto& child : children) {
        if (child->name == childName) {
            return child.get();
        }
    }
    return nullptr;
}

TreeNode* TreeNode::addChild(const std::string& childName, const std::string& childMode) {
    if (TreeNode* existing = findChild(childName)) {
        existing->mode = childMode;
        return existing;
    }
    auto node = std::make_unique<TreeNode>();
    node->name = childName;
    node->mode = childMode;
    TreeNode* ptr = node.get();
    children.push_back(std::move(node));
    return ptr;
}

void insertPath(TreeNode& root, const std::string& path, const std::string& blobHash,
                const std::string& mode) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    if (parts.empty()) return;

    TreeNode* curr = &root;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string& dirName = parts[i];
        TreeNode* child = curr->findChild(dirName);
        if (!child) {
            child = curr->addChild(dirName, kModeDir);
        }
        curr = child;
    }

    const std::string& fileName = parts.back();
    TreeNode* leaf = curr->findChild(fileName);
    if (!leaf) {
        leaf = curr->addChild(fileName, mode);
    } else {
        leaf->mode = mode;
    }
    leaf->hash = blobHash;
}

std::string writeTree(ObjectStore& store, TreeNode& root) {
    std::vector<TreeEntry> entries;
    for (auto& child : root.children) {
        if (child->isDir()) {
            child->hash = writeTree(store, *child);
        }
        entries.push_back(TreeEntry{child->mode, child->name, child->hash});
    }
    root.hash = writeTreeObject(store, entries);
    return root.hash;
}

namespace {
void flattenHelper(ObjectStore& store, const std::string& treeHash, const std::string& prefix,
                   std::vector<FlatEntry>& out) {
    auto entries = readTreeObject(store, treeHash);
    for (const auto& entry : entries) {
        std::string fullPath = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.isDir()) {
            flattenHelper(store, entry.hash, fullPath, out);
        } else {
            out.push_back(FlatEntry{fullPath, entry.mode, entry.hash});
        }
    }
}
}  // namespace

std::vector<FlatEntry> flattenTree(ObjectStore& store, const std::string& treeHash) {
    std::vector<FlatEntry> out;
    flattenHelper(store, treeHash, "", out);
    std::sort(out.begin(), out.end(), [](const FlatEntry& a, const FlatEntry& b) {
        return a.path < b.path;
    });
    return out;
}

// ---------------------------------------------------------------- commit

std::string serializeCommit(const Commit& c) {
    std::string s;
    s += "tree " + c.tree + "\n";
    for (const auto& parent : c.parents) {
        if (!parent.empty()) {
            s += "parent " + parent + "\n";
        }
    }
    std::string authorTz = c.author.tz.empty() ? "+0000" : c.author.tz;
    s += "author " + c.author.name + " <" + c.author.email + "> " +
         std::to_string(c.author.timestamp) + " " + authorTz + "\n";
    std::string committerTz = c.committer.tz.empty() ? "+0000" : c.committer.tz;
    s += "committer " + c.committer.name + " <" + c.committer.email + "> " +
         std::to_string(c.committer.timestamp) + " " + committerTz + "\n";
    s += "\n";
    s += c.message;
    return s;
}

namespace {
bool parseSignature(const std::string& line, const std::string& prefix, Signature& sig) {
    if (line.rfind(prefix, 0) != 0) return false;
    std::string rest = line.substr(prefix.size());
    size_t emailStart = rest.find('<');
    size_t emailEnd = rest.find('>', emailStart);
    if (emailStart == std::string::npos || emailEnd == std::string::npos) {
        return false;
    }
    std::string name = rest.substr(0, emailStart);
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    sig.name = name;
    sig.email = rest.substr(emailStart + 1, emailEnd - emailStart - 1);

    std::string timePart = rest.substr(emailEnd + 1);
    std::istringstream iss(timePart);
    long long ts = 0;
    std::string tz;
    if (iss >> ts >> tz) {
        sig.timestamp = ts;
        sig.tz = tz;
    }
    return true;
}
}  // namespace

Commit parseCommit(const std::string& content) {
    Commit c;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nextNewline = content.find('\n', pos);
        std::string line;
        if (nextNewline == std::string::npos) {
            line = content.substr(pos);
            pos = content.size();
        } else {
            line = content.substr(pos, nextNewline - pos);
            pos = nextNewline + 1;
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            c.message = content.substr(pos);
            return c;
        }

        if (line.rfind("tree ", 0) == 0) {
            c.tree = line.substr(5);
        } else if (line.rfind("parent ", 0) == 0) {
            c.parents.push_back(line.substr(7));
        } else if (line.rfind("author ", 0) == 0) {
            parseSignature(line, "author ", c.author);
        } else if (line.rfind("committer ", 0) == 0) {
            parseSignature(line, "committer ", c.committer);
        }
    }
    return c;
}

std::string writeCommit(ObjectStore& store, const Commit& c) {
    return store.write("commit", serializeCommit(c));
}

Commit readCommit(ObjectStore& store, const std::string& hash) {
    auto opt = store.read(hash);
    if (!opt) {
        throw std::runtime_error("Object not found: " + hash);
    }
    if (opt->type != "commit") {
        throw std::runtime_error("Object " + hash + " is not a commit (is " + opt->type + ")");
    }
    return parseCommit(opt->content);
}

