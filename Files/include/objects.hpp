#pragma once
// Blob / Tree / Commit objects: serialize, parse, and read/write through an ObjectStore.
// These interfaces are shared by every teammate: change them only after telling the team.
#include <memory>
#include <string>
#include <vector>

#include "object_store.hpp"

// ---------------------------------------------------------------- blob
std::string writeBlob(ObjectStore& store, const std::string& content);
std::string readBlob(ObjectStore& store, const std::string& hash);  // throws if not a blob

// ---------------------------------------------------------------- tree
inline constexpr const char* kModeFile = "100644";
inline constexpr const char* kModeExec = "100755";
inline constexpr const char* kModeDir  = "40000";

struct TreeEntry {
    std::string mode;  // kModeFile / kModeExec / kModeDir
    std::string name;  // single path component, no '/'
    std::string hash;  // 40-char hex
    bool isDir() const { return mode == kModeDir; }
};

// Entries are sorted into Git's order before serializing (directories compare as "name/").
std::string serializeTree(std::vector<TreeEntry> entries);
std::vector<TreeEntry> parseTree(const std::string& content);

std::string writeTreeObject(ObjectStore& store, const std::vector<TreeEntry>& entries);
std::vector<TreeEntry> readTreeObject(ObjectStore& store, const std::string& hash);

// ---------------------------------------------------------------- N-ary tree node
// In-memory directory tree used to build tree objects from a flat list of paths.
// Internal nodes are directories, leaves are files.
struct TreeNode {
    std::string name;
    std::string mode;                              // kModeDir for directories
    std::string hash;                              // blob hash (leaf) or tree hash once written
    std::vector<std::unique_ptr<TreeNode>> children;

    bool isDir() const { return mode == kModeDir; }
    TreeNode* findChild(const std::string& childName);
    TreeNode* addChild(const std::string& childName, const std::string& childMode);
};

// Insert "a/b/c.txt" -> blobHash, creating intermediate directory nodes.
void insertPath(TreeNode& root, const std::string& path, const std::string& blobHash,
                const std::string& mode = kModeFile);

// Post-order: writes children first, then each directory. Returns the root tree hash and
// sets node.hash on every directory. Identical subtrees produce identical hashes (reuse).
std::string writeTree(ObjectStore& store, TreeNode& root);

// Pre-order: flatten a tree object into (path, mode, blob hash) entries, sorted by path.
struct FlatEntry {
    std::string path;
    std::string mode;
    std::string hash;
};
std::vector<FlatEntry> flattenTree(ObjectStore& store, const std::string& treeHash);

// ---------------------------------------------------------------- commit
struct Signature {
    std::string name;
    std::string email;
    long long timestamp = 0;   // seconds since epoch
    std::string tz = "+0000";  // "+0530"
};

struct Commit {
    std::string tree;
    std::vector<std::string> parents;  // 0 (root), 1 (normal) or 2 (merge)
    Signature author;
    Signature committer;
    std::string message;
};

std::string serializeCommit(const Commit& c);
Commit parseCommit(const std::string& content);

std::string writeCommit(ObjectStore& store, const Commit& c);
Commit readCommit(ObjectStore& store, const std::string& hash);
