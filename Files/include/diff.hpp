#pragma once
#include <string>
#include <vector>

enum class DiffOp {
    CONTEXT,  // unchanged (' ')
    ADD,      // added ('+')
    DELETE    // deleted ('-')
};

struct DiffLine {
    DiffOp op;
    std::string text;
};

// Splits string into lines (without trailing '\r' or '\n')
std::vector<std::string> splitLines(const std::string& text);

// Computes line diff using Longest Common Subsequence (LCS) dynamic programming
std::vector<DiffLine> computeLineDiff(const std::string& textA, const std::string& textB);

// Formats unified diff output
std::string formatUnifiedDiff(const std::string& pathA, const std::string& pathB,
                              const std::string& textA, const std::string& textB);
