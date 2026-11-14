#include "commands.hpp"
#include "objects.hpp"
#include "object_store.hpp"
#include "sha1.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int cmdHashObject(const std::vector<std::string>& args) {
    bool write = false;
    std::string type = "blob";
    std::string filepath;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-w") {
            write = true;
        } else if (args[i] == "-t") {
            if (i + 1 >= args.size()) {
                std::cerr << "fatal: -t requires an argument\n";
                return 1;
            }
            type = args[++i];
        } else if (!args[i].empty() && args[i][0] == '-') {
            std::cerr << "fatal: unknown option: " << args[i] << "\n";
            return 1;
        } else {
            filepath = args[i];
        }
    }

    if (filepath.empty()) {
        std::cerr << "usage: git hash-object [-w] [-t <type>] <file>\n";
        return 1;
    }

    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        std::cerr << "fatal: Cannot open '" << filepath << "': No such file or directory\n";
        return 1;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (write) {
        auto repo = findRepoDir();
        if (!repo) {
            std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
            return 1;
        }
        ObjectStore store(*repo);
        std::string hash = store.write(type, content);
        std::cout << hash << "\n";
    } else {
        std::string header = type + " " + std::to_string(content.size()) + '\0';
        std::string hash = sha1Hex(header + content);
        std::cout << hash << "\n";
    }

    return 0;
}

int cmdCatFile(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "usage: git cat-file (-p | -t | -s) <object>\n";
        return 1;
    }

    std::string flag = args[0];
    std::string hash = args[1];

    if (!isValidHash(hash)) {
        std::cerr << "fatal: Not a valid object name " << hash << "\n";
        return 1;
    }

    auto repo = findRepoDir();
    if (!repo) {
        std::cerr << "fatal: not a git repository (or any of the parent directories): " << kRepoDirName << "\n";
        return 1;
    }

    ObjectStore store(*repo);
    std::optional<Object> obj;
    try {
        obj = store.read(hash);
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }

    if (!obj) {
        std::cerr << "fatal: Not a valid object name " << hash << "\n";
        return 1;
    }

    if (flag == "-t") {
        std::cout << obj->type << "\n";
    } else if (flag == "-s") {
        std::cout << obj->content.size() << "\n";
    } else if (flag == "-p") {
        if (obj->type == "tree") {
            try {
                auto entries = parseTree(obj->content);
                for (const auto& entry : entries) {
                    std::string modeFormatted = entry.isDir() ? "040000 tree" : (entry.mode + " blob");
                    std::cout << modeFormatted << " " << entry.hash << "\t" << entry.name << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "fatal: failed to parse tree: " << e.what() << "\n";
                return 1;
            }
        } else {
            std::cout.write(obj->content.data(), static_cast<std::streamsize>(obj->content.size()));
            if (!obj->content.empty() && obj->content.back() != '\n') {
                std::cout << "\n";
            }
        }
    } else {
        std::cerr << "fatal: unknown flag: " << flag << "\n";
        return 1;
    }

    return 0;
}

