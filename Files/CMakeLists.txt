cmake_minimum_required(VERSION 3.15)
project(git VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

include_directories(include)

# Storage layer library
set(STORAGE_SOURCES
    src/sha1.cpp
    src/object_store.cpp
    src/objects.cpp
    src/commands/init.cpp
    src/commands/hash_cat.cpp
)

# Main executable
add_executable(git
    src/main.cpp
    ${STORAGE_SOURCES}
)

# Test executable for Teammate A (Storage layer)
add_executable(test_storage
    tests/test_storage.cpp
    ${STORAGE_SOURCES}
)

enable_testing()
add_test(NAME StorageTests COMMAND test_storage)

