#include "diff.hpp"

#include <algorithm>
#include <sstream>

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    if (text.empty()) {
        return lines;
    }
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::vector<DiffLine> computeLineDiff(const std::string& textA, const std::string& textB) {
    auto linesA = splitLines(textA);
    auto linesB = splitLines(textB);

    size_t n = linesA.size();
    size_t m = linesB.size();

    // 2D dynamic programming table for LCS
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));

    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (linesA[i - 1] == linesB[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Backtrack from (n, m) to (0, 0)
    std::vector<DiffLine> diff;
    size_t i = n;
    size_t j = m;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && linesA[i - 1] == linesB[j - 1]) {
            diff.push_back({DiffOp::CONTEXT, linesA[i - 1]});
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            diff.push_back({DiffOp::ADD, linesB[j - 1]});
            --j;
        } else if (i > 0 && (j == 0 || dp[i - 1][j] >= dp[i][j - 1])) {
            diff.push_back({DiffOp::DELETE, linesA[i - 1]});
            --i;
        }
    }

    std::reverse(diff.begin(), diff.end());
    return diff;
}

std::string formatUnifiedDiff(const std::string& pathA, const std::string& pathB,
                              const std::string& textA, const std::string& textB) {
    if (textA == textB) {
        return "";
    }

    std::string out = "diff --git " + pathA + " " + pathB + "\n";
    out += "--- " + pathA + "\n";
    out += "+++ " + pathB + "\n";

    auto diff = computeLineDiff(textA, textB);
    for (const auto& dl : diff) {
        if (dl.op == DiffOp::ADD) {
            out += "+" + dl.text + "\n";
        } else if (dl.op == DiffOp::DELETE) {
            out += "-" + dl.text + "\n";
        } else {
            out += " " + dl.text + "\n";
        }
    }

    return out;
}
