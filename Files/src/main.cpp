#include "commands.hpp"

#include <iostream>
#include <string>
#include <vector>

void printUsage() {
    std::cout << "Usage: git <command> [<args>]\n\n"
              << "Core commands:\n"
              << "   init         Create an empty Git repository\n"
              << "   add          Add file contents to the staging area\n"
              << "   commit       Record changes to the repository\n"
              << "   status       Show the working tree status\n"
              << "   log          Show commit logs\n"
              << "   branch       List, create, or delete branches\n"
              << "   checkout     Switch branches or restore working tree files\n\n"
              << "Storage plumbing commands (Teammate A):\n"
              << "   hash-object  Compute object ID and optionally create a blob from a file\n"
              << "   cat-file     Provide content or type and size information for repository objects\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    if (command == "-h" || command == "--help") {
        printUsage();
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (command == "init") {
        return cmdInit(args);
    } else if (command == "hash-object") {
        return cmdHashObject(args);
    } else if (command == "cat-file") {
        return cmdCatFile(args);
    } else if (command == "add" || command == "commit" || command == "status" ||
               command == "checkout" || command == "log" || command == "branch" ||
               command == "merge" || command == "merge-base" || command == "diff") {
        std::cerr << "git: '" << command << "' is not yet implemented (scheduled for Teammate B/C in Phase 3-7).\n";
        return 1;
    } else {
        std::cerr << "git: '" << command << "' is not a git command. See 'git --help'.\n";
        return 1;
    }
}

