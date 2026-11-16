#include "commit_graph.hpp"

#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

CommitGraph::CommitGraph(ObjectStore& store) : store_(store) {}

std::unordered_set<std::string> CommitGraph::getAncestors(const std::string& startCommit) {
    std::unordered_set<std::string> ancestors;
    if (startCommit.empty()) {
        return ancestors;
    }

    std::queue<std::string> q;
    q.push(startCommit);
    ancestors.insert(startCommit);

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        try {
            Commit c = readCommit(store_, curr);
            for (const auto& p : c.parents) {
                if (!p.empty() && ancestors.insert(p).second) {
                    q.push(p);
                }
            }
        } catch (...) {
            // Missing commit or corrupt object, continue best-effort
        }
    }

    return ancestors;
}

bool CommitGraph::isAncestor(const std::string& ancestor, const std::string& descendant) {
    if (ancestor == descendant) {
        return true;
    }
    auto ancestors = getAncestors(descendant);
    return ancestors.find(ancestor) != ancestors.end();
}

std::optional<std::string> CommitGraph::findMergeBase(const std::string& commitA, const std::string& commitB) {
    if (commitA == commitB) {
        return commitA;
    }

    auto ancestorsA = getAncestors(commitA);
    if (ancestorsA.find(commitB) != ancestorsA.end()) {
        return commitB;
    }

    auto ancestorsB = getAncestors(commitB);
    if (ancestorsB.find(commitA) != ancestorsB.end()) {
        return commitA;
    }

    // BFS from commitB until an ancestor of commitA is reached
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;
    q.push(commitB);
    visited.insert(commitB);

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        if (ancestorsA.find(curr) != ancestorsA.end()) {
            return curr;
        }

        try {
            Commit c = readCommit(store_, curr);
            for (const auto& p : c.parents) {
                if (!p.empty() && visited.insert(p).second) {
                    q.push(p);
                }
            }
        } catch (...) {
        }
    }

    return std::nullopt;
}

std::vector<std::string> CommitGraph::getCommitHistory(const std::string& startCommit) {
    std::vector<std::string> history;
    if (startCommit.empty()) {
        return history;
    }

    struct QueueItem {
        long long timestamp;
        size_t seq;
        std::string hash;

        bool operator<(const QueueItem& other) const {
            if (timestamp != other.timestamp) {
                return timestamp < other.timestamp;  // Max-heap: newest first
            }
            return seq > other.seq;
        }
    };

    std::priority_queue<QueueItem> pq;
    std::unordered_set<std::string> visited;
    size_t seqCounter = 0;

    try {
        Commit start = readCommit(store_, startCommit);
        visited.insert(startCommit);
        pq.push({start.author.timestamp, seqCounter++, startCommit});
    } catch (...) {
        return history;
    }

    while (!pq.empty()) {
        auto item = pq.top();
        pq.pop();
        history.push_back(item.hash);

        try {
            Commit c = readCommit(store_, item.hash);
            for (const auto& parent : c.parents) {
                if (!parent.empty() && visited.insert(parent).second) {
                    try {
                        Commit pCommit = readCommit(store_, parent);
                        pq.push({pCommit.author.timestamp, seqCounter++, parent});
                    } catch (...) {
                        // In case parent timestamp cannot be read
                        pq.push({0, seqCounter++, parent});
                    }
                }
            }
        } catch (...) {
        }
    }

    return history;
}
