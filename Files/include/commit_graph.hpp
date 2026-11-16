#pragma once
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "object_store.hpp"
#include "objects.hpp"

class CommitGraph {
public:
    explicit CommitGraph(ObjectStore& store);

    // Walks history from startCommit (BFS with visited set), ordered newest first by timestamp.
    std::vector<std::string> getCommitHistory(const std::string& startCommit);

    // Finds Lowest Common Ancestor (LCA) of commitA and commitB using ancestor reachability.
    std::optional<std::string> findMergeBase(const std::string& commitA, const std::string& commitB);

    // Returns true if 'ancestor' is reachable by following parent edges from 'descendant'.
    bool isAncestor(const std::string& ancestor, const std::string& descendant);

    // Returns all reachable ancestors of a commit (including itself).
    std::unordered_set<std::string> getAncestors(const std::string& startCommit);

private:
    ObjectStore& store_;
};
