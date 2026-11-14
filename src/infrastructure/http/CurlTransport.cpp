#include "CurlTransport.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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
// Membuat file dengan permission 0600 (POSIX) dan menghapusnya saat destructor dipanggil.
class TempFile {
public:
    explicit TempFile(const std::string& content) {
        try {
            auto temp_dir = std::filesystem::temp_directory_path() / "frans";
            std::filesystem::create_directories(temp_dir);

            auto temp_path = temp_dir / "curl_body_XXXXXX";
            std::string template_str = temp_path.string();

#ifdef _WIN32
            // Windows: gunakan tmpfile_s atau CreateFile
            // Untuk simplicity, gunakan std::tmpfile dan dapatkan path-nya
            // Note: Windows support masih dalam pengembangan (#65)
            char temp_name[L_tmpnam];
            if (tmpnam_s(temp_name, L_tmpnam) != 0) {
                error_ = "Failed to generate temp file name";
                return;
            }
            path_ = temp_name;
            std::ofstream file(path_, std::ios::binary);
            if (!file) {
                error_ = "Failed to create temp file: " + path_;
                return;
            }
            file.write(content.data(), content.size());
            file.close();
#else
            // POSIX: gunakan mkstemp untuk atomik creation
            std::vector<char> template_buf(template_str.begin(), template_str.end());
            template_buf.push_back('\0');

            int fd = mkstemp(template_buf.data());
            if (fd == -1) {
                error_ = "Failed to create temp file with mkstemp";
                return;
            }

            path_ = std::string(template_buf.data());

            // Set permission 0600 (owner read/write only)
            if (fchmod(fd, 0600) != 0) {
                close(fd);
                std::filesystem::remove(path_);
                error_ = "Failed to set temp file permissions";
                return;
            }

            // Write content
            ssize_t written = write(fd, content.data(), content.size());
            close(fd);

            if (written != static_cast<ssize_t>(content.size())) {
                std::filesystem::remove(path_);
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
            // Ignore errors on cleanup
        }
    }

    const std::string& path() const { return path_; }
    const std::string& error() const { return error_; }
    bool valid() const { return !path_.empty() && error_.empty(); }

    // Non-copyable, non-movable
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;

private:
    std::string path_;
    std::string error_;
};

// Escape nilai untuk curl config file.
// Curl config menggunakan format: option = "value"
// Kita perlu escape backslash dan double quote.
std::string escapeCurlConfigValue(const std::string& value) {
    std::string result;
    result.reserve(value.size() * 2);
    for (char c : value) {
        if (c == '\\' || c == '"') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

// Bangun curl config yang akan dikirim via stdin.
// Format:
//   url = "..."
//   header = "..."
//   header = "..."
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

// Parse output curl: baris terakhir adalah status code, sisanya adalah body.
// Format output: <body>\n<status_code>
bool parseCurlOutput(const std::string& output, int& status, std::string& body) {
    if (output.empty()) {
        return false;
    }

    // Cari newline terakhir
    auto last_newline = output.rfind('\n');
    if (last_newline == std::string::npos) {
        // Tidak ada newline, mungkin output hanya status code
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

    // Trim whitespace dari status_str
    status_str.erase(0, status_str.find_first_not_of(" \t\r\n"));
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

    // Validasi input
    if (req.url.empty()) {
        response.error = "URL is empty";
        return response;
    }

    // Buat temp file untuk body (hanya untuk POST/PUT/PATCH)
    std::unique_ptr<TempFile> body_file;
    if (method != "GET" && method != "HEAD") {
        body_file = std::make_unique<TempFile>(req.body);
        if (!body_file->valid()) {
            response.error = "Failed to create temp file for body: " + body_file->error();
            return response;
        }
    }

    // Bangun argv untuk curl
    std::vector<std::string> argv;
    argv.push_back("curl");
    argv.push_back("-sS");  // silent + show error
    argv.push_back("-X");
    argv.push_back(method);
    argv.push_back("-K");
    argv.push_back("-");  // baca config dari stdin

    if (body_file && body_file->valid()) {
        argv.push_back("--data-binary");
        argv.push_back("@" + body_file->path());
    }

    argv.push_back("--max-time");
    argv.push_back(std::to_string(req.timeout_sec));

    // Format output: body + newline + status code
    argv.push_back("-w");
    argv.push_back("\n%{http_code}");

    // Bangun stdin data (curl config dengan secret headers)
    std::string stdin_data = buildCurlConfig(req);

    // Eksekusi curl via ProcessExecutor
    auto result = frans::infrastructure::system::runCommand(
        argv,
        req.timeout_sec + 5,  // timeout sedikit lebih panjang dari curl
        stdin_data,
        "",  // working_dir (default)
        cancel
    );

    // Cek apakah curl tidak ditemukan
    if (result.exit_code == 127 || result.error.find("not found") != std::string::npos ||
        result.error.find("No such file") != std::string::npos) {
        response.error = "curl tidak terpasang. Silakan install curl terlebih dahulu.";
        return response;
    }

    // Cek cancel
    if (result.cancelled) {
        response.cancelled = true;
        response.error = "Request cancelled";
        return response;
    }

    // Cek timeout
    if (result.timed_out) {
        response.timed_out = true;
        response.error = "Request timed out";
        return response;
    }

    // Cek exit code curl
    if (result.exit_code != 0) {
        response.error = "curl failed with exit code " + std::to_string(result.exit_code);
        if (!result.error.empty()) {
            response.error += ": " + result.error;
        }
        return response;
    }

    // Parse output: baris terakhir = status code, sisanya = body
    if (!parseCurlOutput(result.output, response.status, response.body)) {
        response.error = "Failed to parse curl output. Raw output: " + result.output;
        return response;
    }

    return response;
}

} // namespace frans::infrastructure::http