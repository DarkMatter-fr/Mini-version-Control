#pragma once
// Our own hash table: separate chaining, power-of-two buckets, resize at load factor 0.75.
// Average O(1) insert / find / erase.
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// FNV-1a for strings (own hash function); falls back to std::hash for other key types.
template <typename K>
struct DefaultHash {
    size_t operator()(const K& k) const { return std::hash<K>{}(k); }
};

template <>
struct DefaultHash<std::string> {
    size_t operator()(const std::string& s) const {
        size_t h = 1469598103934665603ull;
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return h;
    }
};

template <typename K, typename V, typename Hasher = DefaultHash<K>>
class HashTable {
public:
    explicit HashTable(size_t initialBuckets = 16) : buckets_(roundUpPow2(initialBuckets)) {}

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&&) = default;
    HashTable& operator=(HashTable&&) = default;

    // Returns true if a new key was added, false if an existing key was updated.
    bool insert(const K& key, V value) {
        if (Node* n = findNode(key)) {
            n->value = std::move(value);
            return false;
        }
        if ((size_ + 1) * 4 > buckets_.size() * 3) rehash(buckets_.size() * 2);
        size_t idx = indexFor(key, buckets_.size());
        auto node = std::make_unique<Node>(key, std::move(value));
        node->next = std::move(buckets_[idx]);
        buckets_[idx] = std::move(node);
        ++size_;
        return true;
    }

    V* find(const K& key) {
        Node* n = findNode(key);
        return n ? &n->value : nullptr;
    }
    const V* find(const K& key) const {
        const Node* n = findNode(key);
        return n ? &n->value : nullptr;
    }

    bool contains(const K& key) const { return findNode(key) != nullptr; }

    bool erase(const K& key) {
        size_t idx = indexFor(key, buckets_.size());
        std::unique_ptr<Node>* link = &buckets_[idx];
        while (*link) {
            if ((*link)->key == key) {
                std::unique_ptr<Node> dead = std::move(*link);
                *link = std::move(dead->next);
                --size_;
                return true;
            }
            link = &(*link)->next;
        }
        return false;
    }

    void clear() {
        for (auto& head : buckets_) {
            // Unlink iteratively so a long chain can't overflow the stack on destruction.
            while (head) head = std::move(head->next);
        }
        size_ = 0;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    size_t bucketCount() const { return buckets_.size(); }

    template <typename Fn>
    void forEach(Fn fn) const {
        for (const auto& head : buckets_)
            for (const Node* n = head.get(); n; n = n->next.get()) fn(n->key, n->value);
    }

    ~HashTable() { clear(); }

private:
    struct Node {
        K key;
        V value;
        std::unique_ptr<Node> next;
        Node(const K& k, V v) : key(k), value(std::move(v)) {}
    };

    static size_t roundUpPow2(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    size_t indexFor(const K& key, size_t bucketCount) const { return Hasher{}(key) & (bucketCount - 1); }

    Node* findNode(const K& key) const {
        for (Node* n = buckets_[indexFor(key, buckets_.size())].get(); n; n = n->next.get())
            if (n->key == key) return n;
        return nullptr;
    }

    void rehash(size_t newCount) {
        std::vector<std::unique_ptr<Node>> fresh(newCount);
        for (auto& head : buckets_) {
            while (head) {
                std::unique_ptr<Node> n = std::move(head);
                head = std::move(n->next);
                size_t idx = indexFor(n->key, newCount);
                n->next = std::move(fresh[idx]);
                fresh[idx] = std::move(n);
            }
        }
        buckets_ = std::move(fresh);
    }

    std::vector<std::unique_ptr<Node>> buckets_;
    size_t size_ = 0;
};
