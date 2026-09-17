#include "infrastructure/system/DiffEngine.h"

#include <algorithm>
#include <cstdio>

namespace frans::infrastructure {

namespace {

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    if (s.empty()) return lines;
    std::size_t start = 0;
    while (start < s.size()) {
        const std::size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

} // namespace

FileDiff computeDiff(const std::string& path,
                     const std::string& old_content,
                     const std::string& new_content)
{
    FileDiff result;
    result.path = path;

    if (old_content.empty() && !new_content.empty()) result.is_new_file = true;
    if (!old_content.empty() && new_content.empty()) result.is_deleted  = true;

    const auto old_lines = splitLines(old_content);
    const auto new_lines = splitLines(new_content);

    const int n = static_cast<int>(old_lines.size());
    const int m = static_cast<int>(new_lines.size());

    std::vector<std::vector<int>> dp(
        static_cast<std::size_t>(n + 1),
        std::vector<int>(static_cast<std::size_t>(m + 1), 0));

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (old_lines[static_cast<std::size_t>(i - 1)]
                == new_lines[static_cast<std::size_t>(j - 1)]) {
                dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                    = dp[static_cast<std::size_t>(i - 1)]
                        [static_cast<std::size_t>(j - 1)] + 1;
            } else {
                dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]
                    = std::max(
                        dp[static_cast<std::size_t>(i - 1)]
                          [static_cast<std::size_t>(j)],
                        dp[static_cast<std::size_t>(i)]
                          [static_cast<std::size_t>(j - 1)]);
            }
        }
    }

    std::vector<DiffLine> reversed;
    reversed.reserve(static_cast<std::size_t>(n + m));

    int i = n;
    int j = m;
    while (i > 0 && j > 0) {
        const auto& ol = old_lines[static_cast<std::size_t>(i - 1)];
        const auto& nl = new_lines[static_cast<std::size_t>(j - 1)];
        if (ol == nl) {
            reversed.push_back({DiffLine::Kind::Unchanged, ol});
            --i; --j;
        } else if (dp[static_cast<std::size_t>(i - 1)]
                      [static_cast<std::size_t>(j)]
                   >= dp[static_cast<std::size_t>(i)]
                        [static_cast<std::size_t>(j - 1)]) {
            reversed.push_back({DiffLine::Kind::Removed, ol});
            --i;
        } else {
            reversed.push_back({DiffLine::Kind::Added, nl});
            --j;
        }
    }
    while (i > 0) {
        reversed.push_back({DiffLine::Kind::Removed,
                            old_lines[static_cast<std::size_t>(i - 1)]});
        --i;
    }
    while (j > 0) {
        reversed.push_back({DiffLine::Kind::Added,
                            new_lines[static_cast<std::size_t>(j - 1)]});
        --j;
    }

    std::reverse(reversed.begin(), reversed.end());
    result.lines = std::move(reversed);

    for (const auto& l : result.lines) {
        if (l.kind == DiffLine::Kind::Added)        ++result.added_count;
        else if (l.kind == DiffLine::Kind::Removed) ++result.removed_count;
    }
    return result;
}

std::string formatDiff(const FileDiff& diff) {
    std::string out;
    out += "=== " + diff.path + " ===";
    if (diff.is_new_file) out += "  [NEW FILE]";
    if (diff.is_deleted)  out += "  [DELETED]";
    out += "\n";

    if (diff.added_count == 0 && diff.removed_count == 0) {
        out += "  (no changes)\n";
        return out;
    }

    for (const auto& l : diff.lines) {
        switch (l.kind) {
            case DiffLine::Kind::Unchanged: out += "  " + l.text + "\n"; break;
            case DiffLine::Kind::Added:     out += "+ " + l.text + "\n"; break;
            case DiffLine::Kind::Removed:   out += "- " + l.text + "\n"; break;
        }
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "\n(+%d / -%d)\n",
                  diff.added_count, diff.removed_count);
    out += buf;
    return out;
}

} // namespace frans::infrastructure