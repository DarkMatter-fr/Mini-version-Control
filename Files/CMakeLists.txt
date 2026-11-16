cmake_minimum_required(VERSION 3.15)
project(git VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

include_directories(include)

# Complete engine sources (Storage, Snapshots & History)
set(COMMON_SOURCES
    src/sha1.cpp
    src/object_store.cpp
    src/objects.cpp
    src/index.cpp
    src/refs.cpp
    src/commit_graph.cpp
    src/diff.cpp
    src/commands/init.cpp
    src/commands/hash_cat.cpp
    src/commands/add.cpp
    src/commands/commit.cpp
    src/commands/status.cpp
    src/commands/checkout.cpp
    src/commands/branch.cpp
    src/commands/log.cpp
    src/commands/merge.cpp
    src/commands/diff.cpp
)

# Main CLI executable
add_executable(git
    src/main.cpp
    ${COMMON_SOURCES}
)

# Test executable for Teammate A (Storage layer)
add_executable(test_storage
    tests/test_storage.cpp
    ${COMMON_SOURCES}
)

# Test executable for Teammate B (Snapshots & Indexing layer)
add_executable(test_snapshots
    tests/test_snapshots.cpp
    ${COMMON_SOURCES}
)

# Test executable for Teammate C (History, DAG & Diff layer)
add_executable(test_history
    tests/test_history.cpp
    ${COMMON_SOURCES}
)

enable_testing()
add_test(NAME StorageTests COMMAND test_storage)
add_test(NAME SnapshotsTests COMMAND test_snapshots)
add_test(NAME HistoryTests COMMAND test_history)
