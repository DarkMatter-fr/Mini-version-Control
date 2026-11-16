#pragma once
#include <string>
#include <vector>

// Teammate A (Storage layer)
int cmdInit(const std::vector<std::string>& args);
int cmdHashObject(const std::vector<std::string>& args);
int cmdCatFile(const std::vector<std::string>& args);

// Teammate B (Snapshots & Working directory)
int cmdAdd(const std::vector<std::string>& args);
int cmdCommit(const std::vector<std::string>& args);
int cmdStatus(const std::vector<std::string>& args);
int cmdCheckout(const std::vector<std::string>& args);
