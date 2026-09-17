#include "ui/ManualImportDialog.h"

#include "application/build_runner/BuildRunner.h"
#include "application/manual_import/ManualImporter.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace frans::ui {

using frans::application::BuildPhase;
using frans::application::BuildReport;
using frans::application::BuildRunner;
using frans::application::CommandResult;
using frans::application::GeneratedFile;
using frans::application::ManualImporter;
using frans::application::PhaseResult;
using frans::domain::DangerVerdictLite;
using frans::domain::ImportFilePreview;
using frans::domain::ImportFileStatus;
using frans::domain::ImportResult;
using frans::infrastructure::SandboxedFileSystem;

namespace {

constexpr int kW      = 720;
constexpr int kH      = 660;
constexpr int kPadX   = 15;
constexpr int kInnerW = kW - 2 * kPadX;
constexpr int kBtnH   = 30;
constexpr int kRowGap = 6;

std::string escapeBrowserText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '@': out += "@@"; break;
            case '\n': out += ' '; break;
            case '\t': out += ' '; break;
            default:   out += c;   break;
        }
    }
    return out;
}

std::string formatPreviewLine(const ImportFilePreview& p) {
    const char* tag = nullptr;
    if (p.status == ImportFileStatus::Blocked) {
        tag = "[X] BLOCKED   ";
    } else if (p.status == ImportFileStatus::Overwrite) {
        tag = "[~] OVERWRITE ";
    } else if (p.danger == DangerVerdictLite::NeedsConfirm) {
        tag = "[!] CONFIRM   ";
    } else {
        tag = "[+] NEW       ";
    }

    char size_buf[40];
    std::snprintf(size_buf, sizeof(size_buf), "  (%zu B)", p.size_bytes);

    std::string line = std::string(tag)
                     + escapeBrowserText(p.relative_path) + size_buf;

    if (p.status == ImportFileStatus::Blocked && !p.blocked_reason.empty()) {
        line += "   -- " + escapeBrowserText(p.blocked_reason);
    } else if (p.danger == DangerVerdictLite::NeedsConfirm
               && !p.danger_explanation.empty()) {
        line += "   -- " + escapeBrowserText(p.danger_explanation);
    }
    return line;
}

std::string resolveTilde(const std::string& in) {
    if (in.empty() || in[0] != '~') return in;
    const char* home = std::getenv("HOME");
    if (!home) return in;
    return std::string(home) + in.substr(1);
}

std::string inputValue(const Fl_Input* in) {
    if (!in) return {};
    const char* v = in->value();
    return v ? std::string(v) : std::string{};
}

std::string formatPhase(const PhaseResult& pr) {
    const char* name = frans::application::buildPhaseName(pr.phase);

    std::string s;
    s += "─── [";
    s += name;
    s += "] ───\n";

    if (pr.skipped) {
        s += "  (skipped)\n\n";
        return s;
    }

    s += "  Status: ";
    s += (pr.success ? "SUCCESS" : "FAILED");
    char dur[64];
    std::snprintf(dur, sizeof(dur), "  (%.3fs)\n", pr.duration_sec);
    s += dur;

    for (const auto& cmd : pr.commands) {
        s += "  > " + cmd.command + "\n";
        if (!cmd.success) {
            if (cmd.timed_out) s += "    [TIMEOUT]\n";
            else s += "    [exit " + std::to_string(cmd.exit_code) + "]\n";
        }
    }

    if (!pr.error_message.empty()) s += "  Error: " + pr.error_message + "\n";

    if (!pr.related_files.empty()) {
        s += "  Related files:\n";
        for (const auto& rf : pr.related_files) {
            const std::string marker =
                (rf.role == "PRIMARY_ERROR") ? "    * " : "    - ";
            s += marker + rf.path;
            if (rf.line > 0) s += ":" + std::to_string(rf.line);
            s += "  [" + rf.role + "]";
            if (!rf.note.empty()) s += "\n        " + rf.note;
            s += "\n";
        }
    }

    // Pilih command yang output-nya mau ditampilkan:
    //   - command pertama yang GAGAL (kalau ada), atau
    //   - command TERAKHIR untuk phase Run yang SUKSES
    //     (supaya output program kelihatan — mis. print() Python).
    //
    // Command yang sukses di phase Build/Test tidak dirender supaya
    // tidak noise (compiler output, ctest progress, dsb).
    const CommandResult* show = nullptr;
    for (const auto& cmd : pr.commands) {
        if (!cmd.success) { show = &cmd; break; }
    }
    if (!show && pr.phase == BuildPhase::Run && !pr.commands.empty()) {
        show = &pr.commands.back();
    }

    if (show) {
        const std::string& err_text = show->stderr_tail;
        const std::string& out_text = show->stdout_tail;

        const std::string* text  = nullptr;
        const char*        label = nullptr;

        if (!err_text.empty()) {
            text  = &err_text;
            label = "stderr";
        } else if (!out_text.empty()) {
            text  = &out_text;
            label = "stdout";
        }

        if (text) {
            s += "  ";
            s += label;
            s += ":\n";

            std::string line;
            for (char c : *text) {
                if (c == '\n') {
                    s += "    " + line + "\n";
                    line.clear();
                } else {
                    line += c;
                }
            }
            if (!line.empty()) s += "    " + line + "\n";
        }
    }

    s += "\n";
    return s;
}

std::string formatBuildReport(const BuildReport& r) {
    std::string s;
    s.reserve(2048);

    s += "=== Project Detection ===\n";
    s += "Project root  : " + r.project_root + "\n";
    if (r.project_root != r.sandbox_root) {
        s += "Sandbox root  : " + r.sandbox_root + "\n";
    }
    s += "Build system  : " + r.build_system + "\n";
    s += "Test runner   : "
       + (r.test_runner.empty() ? "(none detected)" : r.test_runner) + "\n";
    s += "Language      : " + r.primary_language + "\n\n";

    s += "=== Overall Result ===\n";
    if (r.overall_success) {
        s += "Status: SUCCESS";
    } else {
        s += "Status: FAILED  (phase: ";
        s += frans::application::buildPhaseName(r.failed_phase);
        s += ")";
    }
    char dur[64];
    std::snprintf(dur, sizeof(dur), "  (total %.3fs)\n\n",
                  r.total_duration_sec);
    s += dur;

    s += formatPhase(r.build);
    s += formatPhase(r.test);
    s += formatPhase(r.run);
    return s;
}

} // namespace

bool ManualImportDialog::show(const std::string& initial_root) {
    ManualImportDialog* dlg = new ManualImportDialog(initial_root);
    dlg->win_->set_modal();
    dlg->win_->show();

    while (dlg->win_->shown()) Fl::wait();

    const bool result = dlg->wrote_anything_;
    delete dlg;
    return result;
}

ManualImportDialog::ManualImportDialog(std::string initial_root)
    : current_root_(std::move(initial_root))
{
    build_runner_ = std::make_unique<BuildRunner>();
    build();
}

ManualImportDialog::~ManualImportDialog() {
    if (win_) {
        if (win_->shown()) win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

void ManualImportDialog::build() {
    win_ = new Fl_Window(kW, kH, "Manual Import — Paste from AI Browser");
    win_->begin();

    int y = kPadX;

    {
        Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 20,
            "Paste AI output — multi-file (File: + block) atau single-file");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 22;
    }

    {
        Fl_Box* l = new Fl_Box(kPadX, y, 100, kBtnH, "Project root:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);

        root_input_ = new Fl_Input(kPadX + 100, y, kInnerW - 100 - 84, kBtnH, nullptr);
        root_input_->value(current_root_.c_str());
        root_input_->tooltip(
            "Folder sandbox. File ditulis di sini. Project root aktual "
            "(untuk build) dideteksi via walk-up dari file ke marker "
            "seperti CMakeLists.txt / package.json.");

        browse_btn_ = new Fl_Button(kPadX + kInnerW - 80, y, 80, kBtnH, "Browse");
        browse_btn_->callback(cbBrowse, this);

        y += kBtnH + kRowGap;
    }

    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16,
            "Paste AI output below  (wajib pakai format \"File: path\" di atas code block):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        l->labelcolor(FL_DARK_BLUE);
        y += 18;
    }

    {
        const int editor_h = 100;
        paste_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
        paste_buf_ = new Fl_Text_Buffer();
        paste_editor_->buffer(paste_buf_);
        paste_editor_->textfont(FL_COURIER);
        paste_editor_->textsize(10);
        paste_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        paste_editor_->color(FL_WHITE);

        paste_buf_->add_modify_callback(onPasteModified, this);

        y += editor_h + kRowGap;
    }

    {
        parse_btn_ = new Fl_Button(kPadX, y, 160, kBtnH, "Parse & Preview");
        parse_btn_->callback(cbParse, this);
        y += kBtnH + kRowGap + 2;
    }

    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16, "Preview:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        y += 18;
    }

    {
        const int browser_h = 70;
        preview_browser_ = new Fl_Browser(kPadX, y, kInnerW, browser_h, nullptr);
        preview_browser_->textfont(FL_COURIER);
        preview_browser_->textsize(10);
        preview_browser_->color(FL_WHITE);
        preview_browser_->type(FL_HOLD_BROWSER);
        y += browser_h + kRowGap;
    }

    {
        status_box_ = new Fl_Box(kPadX, y, kInnerW, 24,
            "Idle. Paste output lalu klik [Parse & Preview].");
        status_box_->box(FL_DOWN_BOX);
        status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        status_box_->labelsize(11);
        y += 24 + kRowGap;
    }

    {
        build_btn_ = new Fl_Button(kPadX, y, 220, kBtnH, "Build / Test / Run");
        build_btn_->callback(cbBuild, this);
        build_btn_->deactivate();
        y += kBtnH + kRowGap;
    }

    {
        build_label_ = new Fl_Box(kPadX, y, kInnerW, 16, "Build output:");
        build_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        build_label_->labelsize(11);
        y += 18;
    }

    {
        const int out_h = 160;
        build_output_ = new Fl_Text_Display(kPadX, y, kInnerW, out_h, nullptr);
        build_buf_ = new Fl_Text_Buffer();
        build_output_->buffer(build_buf_);
        build_output_->textfont(FL_COURIER);
        build_output_->textsize(10);
        build_output_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        build_output_->color(FL_WHITE);
        y += out_h + kRowGap;
    }

    {
        write_btn_ = new Fl_Button(kPadX, y, 160, kBtnH + 4, "Write to Disk");
        write_btn_->callback(cbWrite, this);
        write_btn_->deactivate();

        close_btn_ = new Fl_Button(kPadX + kInnerW - 100, y, 100, kBtnH + 4, "Close");
        close_btn_->callback(cbClose, this);
    }

    win_->end();
}

void ManualImportDialog::rebuildImporter() {
    if (!root_input_) return;

    const std::string raw = inputValue(root_input_);
    if (raw.empty()) {
        fs_.reset();
        importer_.reset();
        current_root_.clear();
        return;
    }

    const std::string resolved = resolveTilde(raw);

    if (resolved == current_root_ && importer_) return;

    if (!current_root_.empty() && resolved != current_root_) {
        resetBuildState("project root berubah");
    }

    current_root_ = resolved;

    try {
        fs_ = std::make_unique<SandboxedFileSystem>(resolved);
    } catch (const std::exception& e) {
        std::fprintf(stderr,
            "[frans][manual_import_dialog] gagal buat sandbox: %s\n", e.what());
        std::fflush(stderr);
        fs_.reset();
        importer_.reset();
        return;
    }

    importer_ = std::make_unique<ManualImporter>(*fs_);
}

void ManualImportDialog::doBrowse() {
    const char* start = root_input_ ? root_input_->value() : nullptr;
    const char* dir = fl_dir_chooser("Pilih project root", start, 0);
    if (dir && *dir && root_input_) {
        root_input_->value(dir);
        resetBuildState("project root diganti via Browse");
    }
}

void ManualImportDialog::doParse() {
    if (!root_input_ || !paste_buf_ || !preview_browser_) return;

    rebuildImporter();

    if (!importer_) {
        fl_message("Project root kosong atau tidak valid.");
        return;
    }

    const std::string text = paste_buf_->text() ? paste_buf_->text() : "";
    if (text.empty()) {
        fl_message("Paste area kosong.");
        return;
    }

    setStatus("Parsing...", FL_DARK_BLUE);
    setButtonsEnabled(false, false);
    Fl::check();

    last_preview_ = importer_->preview(text, "");

    refreshPreviewBrowser(last_preview_);

    if (last_preview_.parse_failed) {
        std::string msg = "Parse FAILED: " + last_preview_.parse_error;
        if (last_preview_.parse_error.find("fallback_path kosong")
            != std::string::npos) {
            msg += "   —   Paste wajib menyertakan baris \"File: <nama_file>\" "
                   "di atas setiap code block.";
        }
        setStatus(msg, FL_RED);
        setButtonsEnabled(true, false);
        return;
    }

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "Parsed: %zu file(s)  |  new=%zu overwrite=%zu blocked=%zu  |  "
        "mode=%s",
        last_preview_.files.size(),
        last_preview_.new_count,
        last_preview_.overwrite_count,
        last_preview_.blocked_count,
        last_preview_.multi_file ? "multi-file" : "single-file (fallback)");

    if (last_preview_.has_blocked()) {
        setStatus(std::string(buf) + "  —  file BLOCKED tidak akan ditulis.",
                  FL_DARK_RED);
    } else {
        setStatus(buf, FL_DARK_GREEN);
    }

    setButtonsEnabled(true, last_preview_.has_writable());
}

void ManualImportDialog::doWrite() {
    if (!importer_ || !importer_->hasPreview()) {
        fl_message("Belum ada preview. Klik [Parse & Preview] dulu.");
        return;
    }
    if (!last_preview_.has_writable()) {
        fl_message("Tidak ada file yang bisa ditulis.");
        return;
    }

    if (last_preview_.overwrite_count > 0) {
        char prompt[256];
        std::snprintf(prompt, sizeof(prompt),
            "Akan menimpa %zu file yang sudah ada.\n\n"
            "Lanjutkan?", last_preview_.overwrite_count);
        const int ans = fl_choice("%s", "Batal", "Ya, timpa", nullptr, prompt);
        if (ans != 1) return;
    }

    setStatus("Writing...", FL_DARK_BLUE);
    setButtonsEnabled(false, false);
    Fl::check();

    const ImportResult res = importer_->commit();

    wrote_anything_ = wrote_anything_ || (res.written_count > 0);

    if (res.written_count > 0 && fs_) {
        if (last_commit_root_ != fs_->root().string()) {
            files_written_.clear();
            last_commit_root_ = fs_->root().string();
        }

        for (const auto& f : res.files) {
            if (f.status == ImportFileStatus::Blocked) continue;
            if (!fs_->exists(f.relative_path)) continue;

            GeneratedFile gf;
            gf.relative_path = f.relative_path;
            gf.content       = f.content;
            files_written_[f.relative_path] = std::move(gf);
        }
    }

    char buf[320];
    if (res.written_count > 0 && res.failed_count == 0) {
        std::snprintf(buf, sizeof(buf),
            "Wrote %zu file(s), %zu blocked. Klik [Build / Test / Run].",
            res.written_count, res.blocked_count);
        setStatus(buf, FL_DARK_GREEN);
    } else if (res.failed_count > 0) {
        std::snprintf(buf, sizeof(buf),
            "Partial: wrote %zu, failed %zu, blocked %zu.",
            res.written_count, res.failed_count, res.blocked_count);
        setStatus(buf, FL_RED);
    } else {
        std::snprintf(buf, sizeof(buf),
            "Tidak ada file ditulis (blocked=%zu).",
            res.blocked_count);
        setStatus(buf, FL_DARK_RED);
    }

    setButtonsEnabled(true, false);
    updateBuildButtonState();
}

void ManualImportDialog::doBuild() {
    if (files_written_.empty()) {
        fl_message("Belum ada file yang di-commit. "
                   "Klik [Parse & Preview] lalu [Write to Disk] dulu.");
        return;
    }
    if (!fs_) {
        fl_message("Sandbox belum siap.");
        return;
    }
    if (is_building_) return;

    std::vector<GeneratedFile> files;
    files.reserve(files_written_.size());
    for (const auto& [path, gf] : files_written_) files.push_back(gf);

    {
        std::vector<GeneratedFile> existing;
        existing.reserve(files.size());
        for (auto& gf : files) {
            if (fs_->exists(gf.relative_path)) existing.push_back(std::move(gf));
        }
        files = std::move(existing);
    }

    if (files.empty()) {
        fl_message("Tidak ada file yang tersedia di sandbox untuk di-build.");
        return;
    }

    is_building_ = true;
    updateBuildButtonState();

    setStatus("Building... (deteksi project root + build system)", FL_DARK_BLUE);
    if (build_buf_) build_buf_->text("");
    Fl::check();

    BuildRunner::ConfirmCallback confirm_cb =
        [](const std::string& reason) -> bool {
            const int ans = fl_choice(
                "%s", "Batal", "Ya, jalankan", nullptr, reason.c_str());
            return ans == 1;
        };

    last_build_report_ = build_runner_->runBuild(
        files,
        fs_->root().string(),
        *fs_,
        120, 120, 30,
        confirm_cb);

    is_building_ = false;

    renderBuildReport(last_build_report_);

    char buf[320];
    if (last_build_report_.overall_success) {
        std::snprintf(buf, sizeof(buf), "SUCCESS — %s @ %s  (%s)",
            last_build_report_.build_system.c_str(),
            last_build_report_.project_root.c_str(),
            last_build_report_.test_runner.empty()
                ? "no test runner" : last_build_report_.test_runner.c_str());
        setStatus(buf, FL_DARK_GREEN);
    } else {
        std::snprintf(buf, sizeof(buf),
            "FAILED pada phase %s — %s @ %s",
            frans::application::buildPhaseName(last_build_report_.failed_phase),
            last_build_report_.build_system.c_str(),
            last_build_report_.project_root.c_str());
        setStatus(buf, FL_RED);
    }

    updateBuildButtonState();
}

void ManualImportDialog::doClose() {
    if (win_) win_->hide();
}

void ManualImportDialog::refreshPreviewBrowser(const ImportResult& r) {
    if (!preview_browser_) return;
    preview_browser_->clear();

    if (r.parse_failed) {
        preview_browser_->add("  (parse gagal — lihat status di bawah)");
        return;
    }
    if (r.files.empty()) {
        preview_browser_->add("  (tidak ada file di output)");
        return;
    }
    for (const auto& f : r.files) {
        preview_browser_->add(formatPreviewLine(f).c_str());
    }
}

void ManualImportDialog::renderBuildReport(const BuildReport& r) {
    if (!build_buf_) return;
    build_buf_->text("");
    const std::string text = formatBuildReport(r);
    build_buf_->append(text.c_str());
    if (build_output_) {
        build_output_->insert_position(build_buf_->length());
        build_output_->show_insert_position();
    }
}

void ManualImportDialog::setStatus(const std::string& msg, unsigned color) {
    if (!status_box_) return;
    status_box_->copy_label(msg.c_str());
    status_box_->labelcolor(static_cast<Fl_Color>(color));
    status_box_->redraw();
}

void ManualImportDialog::setButtonsEnabled(bool parsing_enabled,
                                            bool writing_enabled) {
    if (parse_btn_) {
        if (parsing_enabled) parse_btn_->activate();
        else                 parse_btn_->deactivate();
    }
    if (write_btn_) {
        if (writing_enabled) write_btn_->activate();
        else                 write_btn_->deactivate();
    }
}

void ManualImportDialog::updateBuildButtonState() {
    if (!build_btn_) return;
    const bool can_build = !files_written_.empty() && !is_building_;
    if (is_building_) {
        build_btn_->label("Building...");
        build_btn_->deactivate();
    } else {
        build_btn_->label("Build / Test / Run");
        if (can_build) build_btn_->activate();
        else           build_btn_->deactivate();
    }
    build_btn_->redraw();
}

void ManualImportDialog::resetBuildState(const char* reason) {
    if (files_written_.empty() && !is_building_) return;
    std::fprintf(stderr,
        "[frans][manual_import_dialog] resetBuildState: %s (files_written_=%zu)\n",
        reason ? reason : "(no reason)", files_written_.size());
    std::fflush(stderr);

    files_written_.clear();
    last_commit_root_.clear();
    last_build_report_ = BuildReport{};
    is_building_ = false;

    if (build_buf_) build_buf_->text("");
    updateBuildButtonState();
}

void ManualImportDialog::cbBrowse(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doBrowse();
}
void ManualImportDialog::cbParse(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doParse();
}
void ManualImportDialog::cbWrite(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doWrite();
}
void ManualImportDialog::cbBuild(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doBuild();
}
void ManualImportDialog::cbClose(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doClose();
}

void ManualImportDialog::onPasteModified(int, int nInserted, int nDeleted,
                                          int, const char*, void* cbArg)
{
    auto* self = static_cast<ManualImportDialog*>(cbArg);
    if (!self) return;
    if (nInserted == 0 && nDeleted == 0) return;
    self->resetBuildState("paste area dimodifikasi");
}

} // namespace frans::ui