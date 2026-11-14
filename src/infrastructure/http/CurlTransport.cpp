#include "infrastructure/http/CurlTransport.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "infrastructure/system/ProcessExecutor.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace frans::infrastructure::http {

namespace {

// RAII wrapper untuk file sementara.
// POSIX: dibuat atomik via mkstemp dengan permission 0600.
// Dihapus otomatis saat destructor dipanggil.
class TempFile {
public:
    explicit TempFile(const std::string& content) {
        try {
            auto temp_dir = std::filesystem::temp_directory_path() / "frans";
            std::filesystem::create_directories(temp_dir);

#ifdef _WIN32
            // Windows: dukungan masih dalam pengembangan (#65).
            char temp_name[L_tmpnam_s];
            if (tmpnam_s(temp_name, sizeof(temp_name)) != 0) {
                error_ = "Failed to generate temp file name";
                return;
            }
            path_ = temp_name;
            std::ofstream file(path_, std::ios::binary);
            if (!file) {
                error_ = "Failed to create temp file: " + path_;
                path_.clear();
                return;
            }
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            file.close();
#else
            const std::string template_str =
                (temp_dir / "curl_body_XXXXXX").string();
            std::vector<char> template_buf(template_str.begin(), template_str.end());
            template_buf.push_back('\0');

            const int fd = mkstemp(template_buf.data());
            if (fd == -1) {
                error_ = "Failed to create temp file with mkstemp";
                return;
            }

            path_ = std::string(template_buf.data());

            // Permission 0600 (owner read/write only)
            if (fchmod(fd, 0600) != 0) {
                close(fd);
                std::error_code ec;
                std::filesystem::remove(path_, ec);
                path_.clear();
                error_ = "Failed to set temp file permissions";
                return;
            }

            std::size_t total = 0;
            while (total < content.size()) {
                const ssize_t w = write(fd, content.data() + total,
                                        content.size() - total);
                if (w < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                total += static_cast<std::size_t>(w);
            }
            close(fd);

            if (total != content.size()) {
                std::error_code ec;
                std::filesystem::remove(path_, ec);
                path_.clear();
                error_ = "Failed to write complete content to temp file";
                return;
            }
#endif
        } catch (const std::exception& e) {
            error_ = std::string("Exception creating temp file: ") + e.what();
        }
    }

    ~TempFile() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }

    const std::string& path() const { return path_; }
    const std::string& error() const { return error_; }
    bool valid() const { return !path_.empty() && error_.empty(); }

    TempFile(const TempFile&)            = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&&)                 = delete;
    TempFile& operator=(TempFile&&)      = delete;

private:
    std::string path_;
    std::string error_;
};

// Escape nilai untuk curl config file (format: option = "value").
// Backslash dan double quote di-escape; newline/CR/tab diubah ke escape
// sequence supaya tidak bisa menyuntikkan baris config baru.
std::string escapeCurlConfigValue(const std::string& value) {
    std::string result;
    result.reserve(value.size() * 2);
    for (const char c : value) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    return result;
}

// Bangun curl config yang dikirim via stdin (-K -).
std::string buildCurlConfig(const HttpRequest& req) {
    std::ostringstream config;
    config << "url = \"" << escapeCurlConfigValue(req.url) << "\"\n";

    for (const auto& header : req.headers_secret) {
        config << "header = \"" << escapeCurlConfigValue(header) << "\"\n";
    }
    for (const auto& header : req.headers_plain) {
        config << "header = \"" << escapeCurlConfigValue(header) << "\"\n";
    }
    return config.str();
}

// Parse output curl. Format: <body>\n<status_code>
bool parseCurlOutput(const std::string& output, int& status, std::string& body) {
    if (output.empty()) return false;

    const auto last_newline = output.rfind('\n');
    if (last_newline == std::string::npos) {
        try {
            status = std::stoi(output);
            body.clear();
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string status_str = output.substr(last_newline + 1);
    body = output.substr(0, last_newline);

    const auto first = status_str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return false;
    status_str.erase(0, first);
    status_str.erase(status_str.find_last_not_of(" \t\r\n") + 1);

    try {
        status = std::stoi(status_str);
        return true;
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

HttpResponse CurlTransport::post(const HttpRequest& req, std::atomic<bool>* cancel) {
    return execute(req, "POST", cancel);
}

HttpResponse CurlTransport::get(const HttpRequest& req, std::atomic<bool>* cancel) {
    return execute(req, "GET", cancel);
}

HttpResponse CurlTransport::execute(const HttpRequest& req,
                                    const std::string& method,
                                    std::atomic<bool>* cancel) {
    HttpResponse response;

    if (req.url.empty()) {
        response.error = "URL is empty";
        return response;
    }

    // Temp file untuk body (hanya untuk method selain GET/HEAD).
    std::unique_ptr<TempFile> body_file;
    if (method != "GET" && method != "HEAD") {
        body_file = std::make_unique<TempFile>(req.body);
        if (!body_file->valid()) {
            response.error = "Failed to create temp file for body: " + body_file->error();
            return response;
        }
    }

    std::vector<std::string> argv;
    argv.push_back("curl");
    argv.push_back("-sS");          // silent + show error
    argv.push_back("-X");
    argv.push_back(method);
    argv.push_back("-K");
    argv.push_back("-");            // baca config dari stdin

    if (body_file) {
        argv.push_back("--data-binary");
        argv.push_back("@" + body_file->path());
    }

    argv.push_back("--max-time");
    argv.push_back(std::to_string(req.timeout_sec));

    // Format output: body + newline + status code
    argv.push_back("-w");
    argv.push_back("\n%{http_code}");

    // Secret headers dikirim lewat stdin supaya tidak muncul di ps / Task Manager.
    const std::string stdin_data = buildCurlConfig(req);

    const auto result = frans::infrastructure::runCommand(
        argv,
        req.timeout_sec + 5,   // sedikit lebih panjang dari --max-time curl
        stdin_data,
        "",                    // working_dir (default)
        cancel);

    // execvp gagal -> exit 127 (curl tidak ditemukan di PATH)
    if (result.exit_code == 127) {
        response.error = "curl tidak terpasang. Silakan install curl terlebih dahulu.";
        return response;
    }

    if (result.cancelled_by_user) {
        response.cancelled = true;
        response.error = "Request cancelled";
        return response;
    }

    if (result.timed_out) {
        response.timed_out = true;
        response.error = "Request timed out";
        return response;
    }

    if (result.exit_code != 0) {
        response.error = "curl failed with exit code " + std::to_string(result.exit_code);
        if (!result.stderr_text.empty()) {
            response.error += ": " + result.stderr_text;
        }
        return response;
    }

    if (!parseCurlOutput(result.stdout_text, response.status, response.body)) {
        response.error = "Failed to parse curl output. Raw output: " + result.stdout_text;
        return response;
    }

    return response;
}

} // namespace frans::infrastructure::http
