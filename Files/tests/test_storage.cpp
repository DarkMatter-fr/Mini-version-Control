#include "commands.hpp"
#include "hash_table.hpp"
#include "object_store.hpp"
#include "objects.hpp"
#include "sha1.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond << "\n"; \
            std::exit(1);                                                          \
        }                                                                          \
    } while (0)

void testSha1() {
    std::cout << "[RUN] testSha1\n";

    // Known SHA-1 test vectors
    TEST_ASSERT(sha1Hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    TEST_ASSERT(sha1Hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    TEST_ASSERT(sha1Hex("The quick brown fox jumps over the lazy dog") ==
                "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    // Hex conversions
    std::string raw = sha1Raw("test");
    TEST_ASSERT(raw.size() == 20);
    std::string hex = toHex(raw);
    TEST_ASSERT(hex.size() == 40);
    TEST_ASSERT(fromHex(hex) == raw);
    TEST_ASSERT(isValidHash(hex));
    TEST_ASSERT(!isValidHash("not-a-hash"));
    TEST_ASSERT(!isValidHash("da39a3ee5e6b4b0d3255bfef95601890afd8070G")); // capital G invalid

    bool threw = false;
    try {
        fromHex("123"); // odd length
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_ASSERT(threw);

    std::cout << "[PASS] testSha1\n";
}

void testHashTable() {
    std::cout << "[RUN] testHashTable\n";

    HashTable<std::string, int> table(4);
    TEST_ASSERT(table.empty());
    TEST_ASSERT(table.size() == 0);

    // Insert elements
    TEST_ASSERT(table.insert("apple", 1) == true);
    TEST_ASSERT(table.insert("banana", 2) == true);
    TEST_ASSERT(table.insert("cherry", 3) == true);
    TEST_ASSERT(table.size() == 3);
    TEST_ASSERT(!table.empty());

    // Update existing key
    TEST_ASSERT(table.insert("apple", 10) == false);
    TEST_ASSERT(table.size() == 3);
    int* val = table.find("apple");
    TEST_ASSERT(val != nullptr && *val == 10);

    // Contains & find
    TEST_ASSERT(table.contains("banana"));
    TEST_ASSERT(!table.contains("durian"));
    TEST_ASSERT(table.find("durian") == nullptr);

    // Trigger rehash / resize
    for (int i = 0; i < 50; ++i) {
        table.insert("key_" + std::to_string(i), i * 10);
    }
    TEST_ASSERT(table.size() == 53);
    for (int i = 0; i < 50; ++i) {
        int* v = table.find("key_" + std::to_string(i));
        TEST_ASSERT(v != nullptr && *v == i * 10);
    }

    // Erase
    TEST_ASSERT(table.erase("banana") == true);
    TEST_ASSERT(!table.contains("banana"));
    TEST_ASSERT(table.erase("banana") == false);
    TEST_ASSERT(table.size() == 52);

    // ForEach iteration
    size_t count = 0;
    table.forEach([&count](const std::string&, int) { ++count; });
    TEST_ASSERT(count == 52);

    // Clear
    table.clear();
    TEST_ASSERT(table.empty());
    TEST_ASSERT(table.size() == 0);

    std::cout << "[PASS] testHashTable\n";
}

void testObjectStore() {
    std::cout << "[RUN] testObjectStore\n";

    std::filesystem::path testRepo = std::filesystem::temp_directory_path() / "git_test_store";
    std::error_code ec;
    std::filesystem::remove_all(testRepo, ec);
    std::filesystem::create_directories(testRepo / "objects", ec);

    ObjectStore store(testRepo);

    // 1. Write and read blob with text
    std::string text = "Hello Git!\n";
    std::string hash1 = store.write("blob", text);
    TEST_ASSERT(isValidHash(hash1));
    TEST_ASSERT(store.exists(hash1));

    // Verify directory fan-out on disk: objects/ab/cdef...
    std::filesystem::path diskPath = testRepo / "objects" / hash1.substr(0, 2) / hash1.substr(2);
    TEST_ASSERT(std::filesystem::exists(diskPath));

    // Read back
    auto obj1 = store.read(hash1);
    TEST_ASSERT(obj1.has_value());
    TEST_ASSERT(obj1->type == "blob");
    TEST_ASSERT(obj1->content == text);

    // 2. Binary safety with null bytes
    std::string binaryData;
    binaryData.push_back('\0');
    binaryData.push_back('A');
    binaryData.push_back('\0');
    binaryData.push_back('\xFF');
    binaryData.push_back('\0');

    std::string binHash = store.write("blob", binaryData);
    auto binObj = store.read(binHash);
    TEST_ASSERT(binObj.has_value());
    TEST_ASSERT(binObj->type == "blob");
    TEST_ASSERT(binObj->content == binaryData);

    // 3. Deduplication / Idempotency: writing same content twice creates no second file
    auto lastModTime = std::filesystem::last_write_time(diskPath);
    std::string hashAgain = store.write("blob", text);
    TEST_ASSERT(hashAgain == hash1);
    TEST_ASSERT(std::filesystem::last_write_time(diskPath) == lastModTime);

    // 4. Missing object
    std::string fakeHash = "0123456789abcdef0123456789abcdef01234567";
    TEST_ASSERT(!store.exists(fakeHash));
    auto missing = store.read(fakeHash);
    TEST_ASSERT(!missing.has_value());

    // 5. Corrupt object detection (hash mismatch)
    std::filesystem::path badObjPath = testRepo / "objects" / fakeHash.substr(0, 2) / fakeHash.substr(2);
    std::filesystem::create_directories(badObjPath.parent_path());
    std::ofstream out(badObjPath, std::ios::binary);
    out << "blob 4\0fake";
    out.close();

    bool corruptDetected = false;
    try {
        store.read(fakeHash);
    } catch (const std::runtime_error&) {
        corruptDetected = true;
    }
    TEST_ASSERT(corruptDetected);

    std::filesystem::remove_all(testRepo);
    std::cout << "[PASS] testObjectStore\n";
}

void testObjects() {
    std::cout << "[RUN] testObjects\n";

    std::filesystem::path testRepo = std::filesystem::temp_directory_path() / "git_test_objects";
    std::error_code ec;
    std::filesystem::remove_all(testRepo, ec);
    std::filesystem::create_directories(testRepo / "objects", ec);

    ObjectStore store(testRepo);

    // 1. Blob write & read
    std::string blobContent = "test content\n";
    std::string bHash = writeBlob(store, blobContent);
    TEST_ASSERT(readBlob(store, bHash) == blobContent);

    // Git hash-object parity check for "hello\n":
    // ce013625030ba8dba906f756967f9e9ca394464a
    std::string helloHash = writeBlob(store, "hello\n");
    TEST_ASSERT(helloHash == "ce013625030ba8dba906f756967f9e9ca394464a");

    // 2. Tree serialization and Git sort order
    // In Git, directory entries compare as name + "/"
    std::vector<TreeEntry> entries = {
        {kModeFile, "sub.txt", helloHash},
        {kModeDir, "sub", "aaa96ced2d9a1c8e72c56b253a0e2fe78393feb7"},
        {kModeFile, "sub-2.txt", helloHash}
    };

    std::string serialized = serializeTree(entries);
    auto parsed = parseTree(serialized);
    TEST_ASSERT(parsed.size() == 3);
    // Git sort order: sub-2.txt < sub.txt < sub/
    TEST_ASSERT(parsed[0].name == "sub-2.txt");
    TEST_ASSERT(parsed[1].name == "sub.txt");
    TEST_ASSERT(parsed[2].name == "sub");
    TEST_ASSERT(parsed[2].isDir());

    // Write tree object and read back
    std::string tHash = writeTreeObject(store, entries);
    auto readEntries = readTreeObject(store, tHash);
    TEST_ASSERT(readEntries.size() == 3);
    TEST_ASSERT(readEntries[0].name == "sub-2.txt");

    // 3. TreeNode building and writeTree (post-order bottom-up Merkle tree)
    TreeNode root;
    root.mode = kModeDir;
    insertPath(root, "src/main.cpp", helloHash, kModeFile);
    insertPath(root, "src/util/helper.hpp", helloHash, kModeFile);
    insertPath(root, "README.md", helloHash, kModeFile);

    std::string rootTreeHash = writeTree(store, root);
    TEST_ASSERT(isValidHash(rootTreeHash));
    TEST_ASSERT(store.exists(rootTreeHash));

    // Flatten tree (pre-order traversal)
    auto flat = flattenTree(store, rootTreeHash);
    TEST_ASSERT(flat.size() == 3);
    TEST_ASSERT(flat[0].path == "README.md");
    TEST_ASSERT(flat[1].path == "src/main.cpp");
    TEST_ASSERT(flat[2].path == "src/util/helper.hpp");

    // 4. Commit serialization and parsing
    Commit c;
    c.tree = rootTreeHash;
    c.parents = {"ce013625030ba8dba906f756967f9e9ca394464a"};
    c.author = {"Test Author", "author@test.com", 1700000000, "+0530"};
    c.committer = {"Test Committer", "committer@test.com", 1700000005, "+0530"};
    c.message = "Initial commit\n\nDetailed description here.\n";

    std::string cHash = writeCommit(store, c);
    TEST_ASSERT(isValidHash(cHash));

    Commit readBack = readCommit(store, cHash);
    TEST_ASSERT(readBack.tree == c.tree);
    TEST_ASSERT(readBack.parents.size() == 1 && readBack.parents[0] == c.parents[0]);
    TEST_ASSERT(readBack.author.name == c.author.name);
    TEST_ASSERT(readBack.author.email == c.author.email);
    TEST_ASSERT(readBack.author.timestamp == c.author.timestamp);
    TEST_ASSERT(readBack.author.tz == c.author.tz);
    TEST_ASSERT(readBack.committer.name == c.committer.name);
    TEST_ASSERT(readBack.committer.email == c.committer.email);
    TEST_ASSERT(readBack.committer.timestamp == c.committer.timestamp);
    TEST_ASSERT(readBack.committer.tz == c.committer.tz);
    TEST_ASSERT(readBack.message == c.message);

    std::filesystem::remove_all(testRepo);
    std::cout << "[PASS] testObjects\n";
}

void testInitCommand() {
    std::cout << "[RUN] testInitCommand\n";

    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "git_test_init_repo";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    // 1. Initializing new repository
    int ret1 = cmdInit({tempDir.string()});
    TEST_ASSERT(ret1 == 0);
    TEST_ASSERT(std::filesystem::is_directory(tempDir / kRepoDirName));
    TEST_ASSERT(std::filesystem::is_directory(tempDir / kRepoDirName / "objects"));
    TEST_ASSERT(std::filesystem::is_directory(tempDir / kRepoDirName / "refs" / "heads"));
    TEST_ASSERT(std::filesystem::is_regular_file(tempDir / kRepoDirName / "HEAD"));

    std::string headContent;
    {
        std::ifstream headFile(tempDir / kRepoDirName / "HEAD");
        std::getline(headFile, headContent);
    }
    if (!headContent.empty() && headContent.back() == '\r') {
        headContent.pop_back();
    }
    TEST_ASSERT(headContent == "ref: refs/heads/main");

    // 2. Initializing existing repo must fail cleanly with non-zero exit code
    int ret2 = cmdInit({tempDir.string()});
    TEST_ASSERT(ret2 != 0);

    std::filesystem::remove_all(tempDir, ec);
    std::cout << "[PASS] testInitCommand\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running Teammate A (Storage Layer) Tests\n";
    std::cout << "========================================\n";

    testSha1();
    testHashTable();
    testObjectStore();
    testObjects();
    testInitCommand();

    std::cout << "========================================\n";
    std::cout << "ALL TEAMMATE A TESTS PASSED SUCCESSFULLY\n";
    std::cout << "========================================\n";
    return 0;
}
