// src/application/ingest/MarkdownRenderer.cpp
//
// Fase B #48 -- implementasi MarkdownRenderer.
#include "application/ingest/MarkdownRenderer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace frans::application {

namespace {

// ---------------------------------------------------------------------------
// Tree node internal.
// ---------------------------------------------------------------------------
struct TreeNode {
    std::string name;
    bool is_dir = false;
    bool starred = false;            // hanya bermakna untuk file (is_dir=false)
    std::vector<std::unique_ptr<TreeNode>> children;
};

// Split path "a/b/c.txt" -> {"a", "b", "c.txt"}.
std::vector<std::string> splitPath(const std::string& p) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < p.size()) {
        auto slash = p.find('/', start);
        if (slash == std::string::npos) {
            parts.push_back(p.substr(start));
            break;
        }
        if (slash > start) {
            parts.push_back(p.substr(start, slash - start));
        }
        start = slash + 1;
    }
    return parts;
}

// Bangun tree dari daftar file terurut.
// - Tiap path dipecah jadi komponen; intermediate dirs dibuat otomatis.
// - Children di-sort alphabetical murni (file & dir bercampur).
std::unique_ptr<TreeNode> buildTree(
    const std::vector<CollectedFile>& files,
    const std::string& root_name,
    const std::set<std::string>& starred)
{
    auto root = std::make_unique<TreeNode>();
    root->name = root_name;
    root->is_dir = true;

    for (const auto& f : files) {
        if (f.relative_path.empty()) continue;
        // Skip file yang di-skip (secret/binary/too_large) — tidak
        // muncul di tree. Caller sudah memfilter lewat CollectOptions.
        if (f.skipped) continue;

        const auto parts = splitPath(f.relative_path);
        if (parts.empty()) continue;

        TreeNode* cur = root.get();
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto& part = parts[i];
            const bool is_last = (i == parts.size() - 1);

            TreeNode* child = nullptr;
            for (auto& c : cur->children) {
                if (c->name == part) {
                    child = c.get();
                    break;
                }
            }
            if (!child) {
                auto new_node = std::make_unique<TreeNode>();
                new_node->name = part;
                new_node->is_dir = !is_last;
                if (is_last) {
                    new_node->starred =
                        starred.count(f.relative_path) > 0;
                }
                child = new_node.get();
                cur->children.push_back(std::move(new_node));
            } else if (is_last) {
                // Node sudah ada (mis. karena file lain membuat dir
                // ini lebih dulu). Update starred kalau perlu.
                child->starred = starred.count(f.relative_path) > 0;
            }
            cur = child;
        }
    }

    // Sort children di setiap level: alphabetical murni.
    std::function<void(TreeNode*)> sortChildren = [&](TreeNode* node) {
        std::sort(node->children.begin(), node->children.end(),
                  [](const std::unique_ptr<TreeNode>& a,
                     const std::unique_ptr<TreeNode>& b) {
                      return a->name < b->name;
                  });
        for (auto& c : node->children) {
            if (c->is_dir) sortChildren(c.get());
        }
    };
    sortChildren(root.get());

    return root;
}

// Render satu node tree rekursif.
void renderTreeNode(const TreeNode& node,
                    const std::string& prefix,
                    bool is_last,
                    std::string& out)
{
    out += prefix;
    out += (is_last ? "└── " : "├── ");
    out += node.name;
    if (node.is_dir) {
        out += "/";
    } else if (node.starred) {
        out += " *";
    }
    out += "\n";

    std::string child_prefix = prefix + (is_last ? "    " : "│   ");
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        renderTreeNode(*node.children[i], child_prefix,
                       i == node.children.size() - 1, out);
    }
}

} // namespace

// ===========================================================================
// Public API
// ===========================================================================

std::string renderTree(const std::vector<CollectedFile>& files,
                       const std::string& root_name,
                       const std::vector<std::string>& starred_paths)
{
    std::set<std::string> starred(starred_paths.begin(),
                                  starred_paths.end());
    auto root = buildTree(files, root_name, starred);

    std::string out;
    out += "Directory structure:\n";
    out += "└── ";
    out += root_name;
    out += "/\n";

    std::string child_prefix = "    ";
    for (std::size_t i = 0; i < root->children.size(); ++i) {
        renderTreeNode(*root->children[i], child_prefix,
                       i == root->children.size() - 1, out);
    }
    return out;
}

std::string renderFileBlock(const std::string& path,
                            const std::string& content)
{
    std::string out;
    out += "================================================\n";
    out += "FILE: ";
    out += path;
    out += "\n";
    out += "================================================\n";
    out += content;
    // Pastikan block diakhiri newline supaya block berikutnya
    // mulai di baris baru (penting saat concat multiple blocks).
    if (!content.empty() && content.back() != '\n') {
        out += "\n";
    }
    return out;
}

std::string formatBytesHuman(std::size_t bytes) {
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
                      bytes / (1024.0 * 1024.0));
    }
    return buf;
}

std::string detectLanguageFromExtension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot + 1);
    for (auto& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // C/C++
    if (ext == "cpp" || ext == "cc" || ext == "cxx"
        || ext == "hpp" || ext == "hxx" || ext == "hh") return "cpp";
    if (ext == "c") return "c";
    if (ext == "h") return "c";   // header bisa C atau C++
    // Python
    if (ext == "py" || ext == "pyw" || ext == "pyi") return "python";
    // JS/TS
    if (ext == "js" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "ts" || ext == "tsx") return "typescript";
    if (ext == "vue") return "vue";
    // Data / config
    if (ext == "json") return "json";
    if (ext == "yaml" || ext == "yml") return "yaml";
    if (ext == "toml") return "toml";
    if (ext == "xml") return "xml";
    if (ext == "md") return "markdown";
    if (ext == "txt") return "text";
    // Build
    if (ext == "cmake") return "cmake";
    // Shell
    if (ext == "sh" || ext == "bash") return "bash";
    // Web
    if (ext == "html" || ext == "htm") return "html";
    if (ext == "css" || ext == "scss") return "css";
    // Lain
    if (ext == "sql") return "sql";
    if (ext == "java") return "java";
    if (ext == "go") return "go";
    if (ext == "rs") return "rust";
    if (ext == "rb") return "ruby";
    if (ext == "php") return "php";
    if (ext == "dart") return "dart";
    if (ext == "swift") return "swift";
    if (ext == "kt" || ext == "kts") return "kotlin";
    if (ext == "lua") return "lua";
    if (ext == "r") return "r";
    if (ext == "makefile" || ext == "mk") return "makefile";
    if (ext == "dockerfile") return "dockerfile";
    if (ext == "ini" || ext == "cfg" || ext == "conf") return "ini";

    // Cek nama file khusus (CMakeLists.txt, Makefile, dst).
    const auto slash = path.find_last_of("/\\");
    const std::string basename =
        (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (basename == "CMakeLists.txt") return "cmake";
    if (basename == "Makefile" || basename == "GNUmakefile"
        || basename == "makefile") return "makefile";
    if (basename == "Dockerfile") return "dockerfile";

    return {};
}

std::string renderMarkdown(const RenderInput& in) {
    std::string out;
    out.reserve(4096);

    // ---- Header ----
    out += "# ";
    if (!in.options.header_title.empty()) {
        out += in.options.header_title;
        out += ": ";
    }
    out += in.options.repo_name;
    out += "\n";

    out += "Dibuat oleh Frans";
    if (!in.options.created_at.empty()) {
        out += " | ";
        out += in.options.created_at;
    }
    out += " | ";
    out += std::to_string(in.options.total_files_included);
    out += " file disertakan | ";
    out += formatBytesHuman(in.options.total_content_bytes);
    out += "\n";

    if (!in.options.request_summary.empty()) {
        out += "Permintaan: ";
        out += in.options.request_summary;
        out += "\n";
    }

    out += "Tanda \"*\" pada struktur di bawah = isi file disertakan "
           "setelah bagian ini.\n";
    out += "\n";

    // ---- Tree ----
    out += renderTree(in.all_files, in.options.repo_name,
                      in.starred_paths);
    out += "\n";

    // ---- File blocks ----
    // Sort starred_paths supaya output deterministik (meski caller
    // sudah mengurutkan, kita sort lagi untuk keamanan).
    std::vector<std::string> sorted_starred = in.starred_paths;
    std::sort(sorted_starred.begin(), sorted_starred.end());

    for (const auto& path : sorted_starred) {
        auto it = in.file_contents.find(path);
        if (it == in.file_contents.end()) {
            // Path di starred tapi konten tidak ada — skip dengan
            // warning ke stderr (caller seharusnya mengisi semua).
            std::fprintf(stderr,
                "[frans][markdown_renderer] warning: starred path '%s' "
                "tidak ada di file_contents — dilewati.\n",
                path.c_str());
            std::fflush(stderr);
            continue;
        }
        out += renderFileBlock(path, it->second);
    }

    return out;
}

} // namespace frans::application
