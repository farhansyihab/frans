# Blueprint Tambahan: Frans v3: Ingest, Git, dan Prompt Generator

**Versi**: 3.0-draft (addendum untuk BluePrint-v8 / Frans v2.1)
**Status**: Perencanaan. Belum ada kode yang ditulis.
**Hubungan dengan v8**: Dokumen ini **tidak mengubah** apa pun yang sudah jadi di v8. Semua modul v8 (Parser, ManualImporter, Sandbox, DangerCheck, BuildRunner, dll.) tetap berlaku. Dokumen ini hanya **menambah** modul baru.
**Target Platform**: Windows 10/11 + Lubuntu 22.04+ (sama dengan v8)
**Toolkit**: C++20 + FLTK 1.3 + CMake (sama dengan v8)

---

## 1. Latar Belakang & Tujuan

Workflow saat ini (v2.1):

```
[gitingest.com / manual]  ->  upload ke browser AI  ->  tulis prompt manual
        ->  AI jawab  ->  paste ke Frans  ->  Preview/Write/Build
        ->  git add/commit/push manual di terminal
```

Tiga langkah di sekitar inti Frans masih manual: mengambil konteks repo, menyusun prompt, dan git. Frans v3 menutup ketiganya:

```
   ┌─────────────┐   ┌──────────────┐   ┌───────────────┐
   │   INGEST    │   │    PROMPT    │   │ (browser AI)  │
   │ repo -> .md │──▶│ generate teks│──▶│ upload .md +  │
   │ (lokal)     │   │ (Copy manual)│   │ paste prompt  │
   └─────────────┘   └──────────────┘   └───────┬───────┘
                                                │ jawaban AI
   ┌─────────────┐   ┌──────────────┐           ▼
   │     GIT     │◀──│ BUILD / RUN  │◀──  IMPORT (v2.1, sudah jadi)
   │ status/add/ │   │ (v2.1)       │
   │ commit/push │   └──────────────┘
   └─────────────┘
```

### 1.1 Target penggunaan

Frans v3 ditujukan untuk **aplikasi kecil** (puluhan file, bukan ribuan). Konsekuensinya:
- Satu file ingest muat di konteks browser AI.
- Batas ukuran default konservatif.
- Tidak ada fitur untuk monorepo besar.

### 1.2 Prinsip yang tetap dipegang (dari v8)

- **Tidak ada AI call.** Frans tidak pernah bicara ke API AI. Prompt hanya **dibuat** sebagai teks; pengguna yang menyalin dan menempelnya ke browser.
- **Tidak ada auto-inject** ke browser (keputusan final, lihat §9).
- **Safety layer tetap**: sandbox, danger check, konfirmasi eksplisit untuk aksi berisiko.
- **Personal use**: git clone + build, tanpa installer.

---

## 2. Scope

### 2.1 IN

| Komponen | Keterangan |
|---|---|
| `RepoIngester` | Generate file markdown ala gitingest, 100% lokal, tanpa jaringan |
| `GitFacade` | `git status`, `add`, `commit`, `push` via subprocess |
| `PromptGenerator` | Render template + permintaan pengguna menjadi teks prompt siap copy |
| `SessionState` | State bersama antar-dialog (mis. error build terakhir) |
| `IngestDialog`, `GitDialog`, `PromptDialog` | UI baru, pola sama dengan `ManualImportDialog` |
| `ProcessExecutor` + parameter `env` | Dibutuhkan git (`GIT_TERMINAL_PROMPT=0`) |
| Perluasan `DangerCheckHandler` | Pola git berisiko |
| Perluasan `AppConfig` | Key string/int baru |
| Test scenario baru | Di `ScenarioTest.cpp` / `Tests.cmake` |

### 2.2 OUT (tidak dikerjakan di v3)

| Komponen | Alasan |
|---|---|
| Auto-inject / auto-paste ke browser | Rapuh (fokus jendela, Wayland), lihat §9 |
| Userscript / ekstensi browser | Proyek tersendiri |
| Auto-copy ke clipboard saat generate | Pengguna memilih menyalin sendiri; tombol Copy manual tetap ada (§5.3) |
| GitHub API (PR, issue, create repo) | `git push` cukup; auth lewat SSH/credential helper |
| Branch management, merge, rebase, stash | Di luar kebutuhan app kecil |
| `git push --force`, `reset --hard` dari UI | Tidak disediakan tombolnya; kalau terdeteksi di command, `NeedsConfirm` |
| Parser `.gitignore` buatan sendiri | Git yang menangani (§4.1); fallback pakai exclude bawaan |
| Hitung token akurat | Hanya estimasi kasar (byte / 4) |
| Chunking ingest otomatis | App kecil; cukup peringatan jika terlalu besar |

---

## 3. Keputusan Arsitektur Kunci

| Keputusan | Alasan |
|---|---|
| **Ingest pakai `git ls-files` sebagai jalur utama** | Git sudah menangani semantik `.gitignore` penuh (negasi `!`, `**`, nested `.gitignore`, global ignore). Menulis ulang hampir pasti ada bug. |
| **Fallback walker `std::filesystem`** saat bukan repo git | Tetap berguna untuk folder tanpa git; pakai daftar exclude bawaan. |
| **Format output ingest mirip gitingest** | Pengguna sudah terbiasa; AI sudah paham format ini dari pengalaman sebelumnya. |
| **Secret di-exclude default** (`.env`, `*.pem`, `*.key`, dll.) | File ingest di-upload ke layanan pihak ketiga. Kebocoran kredensial adalah risiko nyata. |
| **Dialog terpisah per fitur, bukan `Fl_Tabs`** | *Merevisi usulan awal di percakapan.* `ManualImportDialog` sudah besar dan stabil; merombaknya jadi tab berisiko regresi. Dialog baru memakai pola `show(initial_root)` yang sama, nol sentuhan ke kode yang sudah tervalidasi. |
| **`SessionState` kecil di `MainWindow`** | Dialog Prompt perlu "error build terakhir", padahal `ManualImportDialog` dihancurkan saat ditutup. Satu struct bersama menyelesaikan ini tanpa singleton global. |
| **Operasi git di worker thread** | Pola sama dengan build: UI responsif, tombol Cancel, polling `Fl::add_timeout`. |
| **Konfirmasi aksi berisiko di UI thread, sebelum worker dimulai** | `confirm_cb` di worker auto-approve (keputusan v8 §3). Untuk git, dialog konfirmasi harus muncul **sebelum** thread dijalankan. |
| **`GIT_TERMINAL_PROMPT=0`** | `ProcessExecutor` memberi stdin `/dev/null`; tanpa ini git bisa menggantung menunggu password. Dengan ini git gagal cepat dengan pesan jelas. |
| **Template prompt di file (`config/prompt_template.txt`)** | Mudah diedit tanpa rebuild; isi default = prompt yang selama ini dipakai. |

---

## 4. Modul Inti

### 4.1 RepoIngester (`application/ingest/`)

**Tugas**: Mengubah folder project menjadi satu file markdown berisi struktur direktori + isi semua file teks.

**API (rancangan)**:
```cpp
namespace frans::application {

struct IngestOptions {
    std::size_t max_file_bytes  = 256 * 1024;       // per file
    std::size_t max_total_bytes = 5 * 1024 * 1024;  // total isi file
    bool        exclude_secrets = true;
    std::vector<std::string> extra_excludes;        // glob sederhana, opsional
    std::string output_dir;                          // dari AppConfig
};

struct IngestSkipped {
    std::string path;
    std::string reason;   // "binary" | "terlalu besar" | "secret" | "melebihi total" | "bukan UTF-8"
};

struct IngestResult {
    bool        ok = false;
    std::string error;
    std::string output_path;           // file .md yang ditulis
    std::size_t files_included = 0;
    std::size_t total_bytes    = 0;
    std::size_t est_tokens     = 0;    // total_bytes / 4, kasar
    bool        used_git       = false;
    std::vector<IngestSkipped> skipped;
};

IngestResult ingestRepo(const std::string& project_root,
                        const IngestOptions& opt,
                        std::atomic<bool>* cancel_flag = nullptr);

}
```

**Alur**:
1. **Kumpulkan daftar file.**
   - Jalur utama: `git -C <root> ls-files -z --cached --others --exclude-standard`. Opsi `-z` membuat nama file dengan spasi/unicode aman. Ini mencakup file tracked + untracked yang tidak di-ignore, jadi file baru hasil import AI ikut masuk sebelum di-commit.
   - Fallback (bukan repo git / git tidak ada): walker `std::filesystem` dengan exclude bawaan: `.git`, `build`, `node_modules`, `__pycache__`, `.venv`, `venv`, `dist`, `.idea`, `.vscode`, `target`.
2. **Saring tiap file** (urutan pemeriksaan):
   - tidak ada di disk (tracked tapi sudah dihapus) → lewati diam-diam
   - symlink / submodule → lewati
   - cocok pola secret → skip, alasan `secret`
   - file output ingest itu sendiri (dan seluruh `output_dir` bila di dalam repo) → lewati
   - ukuran > `max_file_bytes` → skip, alasan `terlalu besar`
   - ada byte NUL di 8 KB pertama → skip, alasan `binary`
   - total kumulatif akan melewati `max_total_bytes` → skip sisanya, alasan `melebihi total`
3. **Urutkan** path secara leksikografis (deterministik; hasil sama untuk input sama).
4. **Render** ke markdown (format di bawah) dan tulis ke `output_dir`.

**Pola secret default** (nama file / basename):
`.env`, `.env.*`, `*.pem`, `*.key`, `*.p12`, `*.pfx`, `id_rsa`, `id_ed25519`, `*.keystore`, `credentials.json`, `secrets.*`.

**Format output** (mengikuti gitingest):

````plaintext
Directory structure:
└── nama-repo/
    ├── README.md
    ├── src/
    │   └── main.py
    └── tests/
        └── test_main.py


================================================
FILE: README.md
================================================
<isi file>


================================================
FILE: src/main.py
================================================
<isi file>
````

- Baris pertama (opsional) berisi ringkasan: jumlah file, total byte, estimasi token.
- Bagian akhir (opsional) daftar file yang di-skip + alasannya, agar pengguna tahu apa yang tidak dilihat AI.

**Nama file output**: `<nama-repo>-ingest.md`, ditimpa setiap kali dijalankan (tanpa timestamp, supaya path stabil untuk upload berulang).

**Catatan**: file ingest berisi banyak baris berawalan `FILE:`. Ini aman karena file ini **di-upload ke AI**, bukan di-paste ke `MultiFileParser` Frans. Jangan paste file ingest ke paste area Import.

**Test**: temp dir berisi file teks, file biner, file besar, `.env`, subfolder; cek yang masuk/skip. Test dengan `git init` dijalankan hanya jika `git` tersedia, kalau tidak → SKIP (bukan FAIL).

### 4.2 GitFacade (`application/git/`)

**Tugas**: Pembungkus tipis perintah git. Tidak menyimpan state repo; setiap panggilan membaca keadaan terkini.

**API (rancangan)**:
```cpp
namespace frans::application {

struct GitFileChange {
    char        index_status;     // kolom X dari porcelain v1
    char        worktree_status;  // kolom Y
    std::string path;
    bool        suspicious = false;  // cocok pola secret
};

struct GitStatus {
    bool        is_repo = false;
    bool        git_available = true;
    std::string branch;
    std::string upstream;          // kosong jika tidak ada
    int         ahead = 0;
    int         behind = 0;
    std::vector<GitFileChange> changes;
    std::string error;
    bool clean() const { return is_repo && changes.empty(); }
};

struct GitOpResult {
    bool        ok = false;
    int         exit_code = -1;
    bool        cancelled = false;
    std::string output;            // stdout+stderr, di-tail
    std::string error;
};

class GitFacade {
public:
    explicit GitFacade(std::string project_root);

    GitStatus   status(std::atomic<bool>* cancel = nullptr);
    GitOpResult addAll(std::atomic<bool>* cancel = nullptr);
    GitOpResult commit(const std::string& message,
                       std::atomic<bool>* cancel = nullptr);
    GitOpResult push(std::atomic<bool>* cancel = nullptr);

    // Murni (tanpa subprocess), mudah dites:
    static GitStatus parseStatusPorcelain(const std::string& text);
    static std::string suggestCommitMessage(const GitStatus& st);
};

}
```

**Perintah yang dipakai**:

| Operasi | Perintah | Catatan |
|---|---|---|
| status | `git status --porcelain=v1 -b -z` *(atau tanpa `-z` bila parsing rename dibuat sederhana)* | Baris `## branch...upstream [ahead N, behind M]` diparse untuk branch/ahead/behind |
| add | `git add -A` | Didahului pemeriksaan file mencurigakan (lihat bawah) |
| commit | `git commit -m <message>` | `-m` sebagai argv terpisah, **bukan** lewat shell, jadi tidak ada injection |
| push | `git push` | Memakai upstream yang sudah di-set. Jika belum ada upstream, tampilkan pesan jelas dan petunjuk `git push -u origin <branch>` (tidak dijalankan otomatis di fase awal) |

Semua perintah dijalankan dengan `cwd = project_root`, `env: GIT_TERMINAL_PROMPT=0`, via `ProcessExecutor`.

**Safety**:
- **Pemeriksaan file mencurigakan sebelum `add -A`**: jika `status()` menemukan file cocok pola secret yang **tidak** di-ignore, dialog peringatan muncul: *"File berikut akan ikut ter-commit: .env, ..."* dengan pilihan Batal / Lanjut. Ini lapisan kedua setelah exclude di ingest.
- **Commit tanpa perubahan** ditolak di UI (tombol disabled bila `clean()`).
- **Push selalu lewat dialog konfirmasi** yang menampilkan branch, upstream, dan jumlah commit yang akan dikirim (`ahead`).
- **Tidak ada tombol** untuk force-push, reset, clean, checkout. Bila seseorang memanggil command seperti itu lewat jalur `BuildRunner`, `DangerCheckHandler` menandainya (§4.5).
- Konfirmasi dilakukan di **UI thread sebelum worker dimulai**.

**Autentikasi push**: Frans tidak pernah menanyakan atau menyimpan kredensial. Prasyarat pengguna:
- Linux: SSH key (agent berjalan) atau `gh auth login` / credential helper.
- Windows: Git Credential Manager (bawaan Git for Windows).

Jika push gagal karena auth, output git ditampilkan apa adanya plus petunjuk satu baris.

**Pesan commit otomatis** (`suggestCommitMessage`): contoh `frans: update 3 file (1 baru)`. Hanya saran awal di kotak teks; pengguna bebas mengubah.

**Bonus (Fase D)**: snapshot commit otomatis sebelum "Write to Disk" menimpa file (menyelesaikan item rollback #35 di v8 secara murah). Opsional lewat config, default mati.

### 4.3 PromptGenerator (`application/prompt/`)

**Tugas**: Membuat teks prompt dari template + isi permintaan pengguna. **Murni fungsi teks**: tidak ada clipboard, tidak ada jaringan, tidak ada filesystem di dalam fungsi render (template dibaca oleh pemanggil).

**API (rancangan)**:
```cpp
namespace frans::application {

struct PromptInput {
    std::string tmpl;            // isi template
    std::string request;         // permintaan pengguna
    std::string error_output;    // opsional: output error build/run
};

// Placeholder yang didukung di template:
//   {{REQUEST}}        -> request
//   {{ERROR_OUTPUT}}   -> error_output (kosong jika tidak ada)
// Blok kondisional (opsional):
//   {{#ERROR}} ... {{/ERROR}}  -> hanya dirender jika error_output tidak kosong
std::string renderPrompt(const PromptInput& in);

}
```

**Template default** (`config/prompt_template.txt`): berisi aturan format `File:` yang selama ini dipakai, diakhiri:

````plaintext
...
8. Full file content, not diff

Then output the code for my request below.

--- my request ---
{{REQUEST}}
{{#ERROR}}

--- error output ---
{{ERROR_OUTPUT}}
{{/ERROR}}
````

(Isi lengkap = prompt pengguna saat ini, dipindah ke file template apa adanya, termasuk aturan 1–8 dan instruksi Bahasa Indonesia.)

**Perilaku**:
- Template tidak ditemukan → pakai template bawaan yang di-embed di source (string konstan), log peringatan. Aplikasi tidak boleh gagal hanya karena template hilang.
- Placeholder tak dikenal dibiarkan apa adanya (tidak dihapus), agar kesalahan ketik terlihat di preview.
- Blok `{{#ERROR}}` tanpa penutup → dirender apa adanya + warning di status.
- **Tidak ada pelarian karakter**: isi `request` dan `error_output` disisipkan mentah. Peringatan di UI jika `error_output` mengandung tiga backtick berurutan (bisa merusak format aturan #7 di prompt).
- `error_output` di-tail (maks ~8 KB) agar prompt tidak membengkak oleh log panjang.

**Test**: render dengan/tanpa error; blok kondisional; placeholder tak dikenal; template kosong; request berisi karakter khusus.

### 4.4 SessionState (`ui/` atau `application/session/`)

```cpp
struct SessionState {
    std::string last_project_root;
    std::string last_build_error;     // ringkasan error dari BuildReport terakhir
    std::string last_ingest_path;
};
```

- Dimiliki `MainWindow`; pointer diteruskan ke tiap dialog.
- `ManualImportDialog::show(...)` menerima pointer opsional `SessionState*`; setelah build selesai (gagal) ia menulis ringkasan error: phase, `error_message`, related files `path:line`, dan tail stderr command yang gagal. Ini perubahan **kecil dan aditif** di dialog yang sudah ada. Kalau pointer null, perilaku lama tidak berubah.
- `PromptDialog` membaca `last_build_error` untuk tombol "Ambil dari build terakhir".

### 4.5 Perluasan DangerCheckHandler

`checkCommand()` saat ini hanya melihat base command dan flag `rm`. Tambahan:

| Pola | Verdict |
|---|---|
| `git push` dengan `--force` / `-f` / `--force-with-lease` | NeedsConfirm |
| `git reset --hard` | NeedsConfirm |
| `git clean` dengan `-f` | NeedsConfirm |
| `git checkout .` / `git restore .` | NeedsConfirm |
| `git config` yang menyentuh `credential` | NeedsConfirm |

Karena `GitFacade` hanya membangun argv yang aman (add/commit/push biasa), pola ini terutama jaring pengaman untuk jalur command umum. Test unit di `ScenarioTest`.

### 4.6 Perubahan ProcessExecutor

Tambah parameter di akhir signature (kompatibel mundur; pemanggil lama tidak berubah):

```cpp
ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec = 30,
                         const std::string& stdin_data = {},
                         const std::string& working_dir = {},
                         std::atomic<bool>* cancel_flag = nullptr,
                         const std::vector<std::string>& env_extra = {}); // "KEY=VALUE"
```

- **POSIX**: di child, setelah `fork` dan sebelum `execvp`, `putenv`/`setenv` tiap entri (aman karena di child).
- **Win32**: bangun environment block (`GetEnvironmentStringsW` + entri tambahan, diakhiri `\0\0`) dan teruskan ke `CreateProcessW` dengan flag `CREATE_UNICODE_ENVIRONMENT`. **Jangan** pakai `SetEnvironmentVariable` di proses induk (tidak thread-safe terhadap worker lain).
- ⚠️ Sisi Win32 mewarisi status "belum diuji" dari v8 §9.2 #21.

### 4.7 UI

| Dialog | Isi |
|---|---|
| **IngestDialog** | Project root (dari `MainWindow`), checkbox "Exclude secret", field batas ukuran (default dari config), tombol **Generate Ingest**, tombol Cancel, area hasil: path output, jumlah file, total byte, estimasi token, daftar file yang di-skip + alasan, tombol **Buka Folder Output** |
| **GitDialog** | Panel status (branch, ahead/behind, daftar perubahan dengan penanda `[!]` untuk file mencurigakan), tombol **Refresh**, **Add All**, kotak pesan commit (terisi saran), **Commit**, **Push** (dengan konfirmasi), area output git |
| **PromptDialog** | Kotak "Permintaan saya", kotak "Error output" (opsional) + tombol "Ambil dari build terakhir", tombol **Generate Prompt**, area **preview hasil** (read-only, bisa di-select-all), tombol **Copy** (opsional/pelengkap, `Fl::copy`), info jumlah karakter |

`MainWindow` mendapat tiga tombol baru di samping **Open Import Dialog**: **Ingest…**, **Git…**, **Prompt…**.

**Catatan PromptDialog**: sesuai keputusan Anda, *tidak ada* centang auto-inject dan *tidak ada* auto-copy. Alur: isi permintaan → **Generate Prompt** → baca preview → salin sendiri (atau tombol Copy) → tempel di browser. Tombol Copy hanya jalan pintas; tidak dipanggil otomatis.

### 4.8 Perubahan AppConfig

Key baru di `config/app.json` (flat, sesuai keterbatasan parser v8 §6.10):

```json
{
  "ingest_output_dir": "~/frans-out",
  "ingest_max_file_kb": 256,
  "ingest_max_total_mb": 5,
  "prompt_template_path": "config/prompt_template.txt"
}
```

- Parser saat ini hanya menerima **dua key string** (`window_title`, `default_project_root`); perlu diperluas untuk `ingest_output_dir` dan `prompt_template_path`.
- `~` di-resolve seperti `resolveTilde` di `ManualImportDialog`; di Windows pakai `USERPROFILE`.
- `prompt_template_path` relatif di-resolve lewat logika `AppPaths` (cari di CWD, `exe_dir`, walk-up), sama seperti `app.json`.
- **Anomali yang ditemukan saat membaca source**: `config/app.json` memakai key `"title"` di dalam objek `window`, tetapi parser mencari `"window_title"`, jadi judul dari config tidak pernah terbaca (kebetulan tidak terlihat karena default juga `"Frans"`). Perbaiki di Fase 0: samakan key (`"title"`) di parser.

---

## 5. Struktur File Baru

```
frans/
├── cmake/
│   ├── Sources.cmake                 (EDIT: tambah file baru)
│   └── Tests.cmake                   (EDIT: tambah sumber test baru)
├── config/
│   ├── app.json                      (EDIT: key baru)
│   └── prompt_template.txt           (BARU)
├── src/
│   ├── application/
│   │   ├── ingest/
│   │   │   └── RepoIngester.cpp/.h           (BARU)
│   │   ├── git/
│   │   │   └── GitFacade.cpp/.h              (BARU)
│   │   ├── prompt/
│   │   │   └── PromptGenerator.cpp/.h        (BARU)
│   │   └── safety/
│   │       └── DangerCheckHandler.cpp        (EDIT: pola git)
│   ├── infrastructure/system/
│   │   ├── ProcessExecutor.h                 (EDIT: env_extra)
│   │   ├── ProcessExecutor_posix.cpp         (EDIT)
│   │   └── ProcessExecutor_win32.cpp         (EDIT)
│   ├── config/
│   │   └── AppConfig.cpp/.h                  (EDIT: key baru)
│   └── ui/
│       ├── SessionState.h                    (BARU)
│       ├── MainWindow.cpp/.h                 (EDIT: 3 tombol + SessionState)
│       ├── ManualImportDialog.cpp/.h         (EDIT kecil: tulis last_build_error)
│       ├── IngestDialog.cpp/.h               (BARU)
│       ├── GitDialog.cpp/.h                  (BARU)
│       └── PromptDialog.cpp/.h               (BARU)
└── tests/
    └── ScenarioTest.cpp                      (EDIT: scenario baru)
```

Estimasi: ±14 file baru, ±10 file diedit. Binary tetap jauh di bawah 5 MB.

**Aturan dependensi** (tetap seperti v8 §4): UI → Application → Infrastructure. `RepoIngester`, `GitFacade`, `PromptGenerator` tidak boleh meng-include header FLTK.

---

## 6. Threading

| Operasi | Thread | Cancel |
|---|---|---|
| Ingest | Worker (ada subprocess `git ls-files` + I/O file) | `cancel_flag` dicek tiap file |
| Git status/add/commit/push | Worker | `cancel_flag` → `ProcessExecutor` (SIGKILL / TerminateProcess ≤200 ms) |
| Prompt render | UI thread (cepat, murni teks) | Tidak perlu |

Pola identik dengan `ManualImportDialog`: `std::thread` + `std::atomic<bool>` + polling `Fl::add_timeout(0.1s)`. Worker **tidak** menyentuh FLTK; hasil ditaruh di member, dibaca UI setelah flag `finished`.

⚠️ Hindari dua worker aktif untuk **repo yang sama** (mis. Git add saat Build berjalan). Setiap dialog bersifat modal seperti `ManualImportDialog`, jadi dalam praktik hanya satu dialog aktif; tidak perlu lock tambahan.

---

## 7. Safety Layer (tambahan di atas v8 §8)

1. **Secret exclusion** di ingest (default aktif).
2. **Peringatan file mencurigakan** sebelum `git add -A`.
3. **Konfirmasi eksplisit** untuk push (di UI thread, sebelum worker).
4. **Tidak ada operasi destruktif git** di UI (force, reset, clean).
5. **`GIT_TERMINAL_PROMPT=0`**: git tidak pernah menggantung menunggu input.
6. **Tanpa shell**: semua command dibangun sebagai argv (bukan string shell), jadi pesan commit dengan karakter aneh tidak bisa jadi injection.
7. **Output ingest di luar repo secara default** (`~/frans-out`) agar tidak ikut ter-commit; kalau pengguna memilih lokasi di dalam repo, file itu di-exclude dari ingest dan `GitDialog` memperingatkan jika ia muncul di status.
8. **Tidak menyimpan kredensial**. Auth sepenuhnya urusan git/OS.

---

## 8. Roadmap

Penomoran melanjutkan v8 (item terakhir v8 yang disebut = #36).

### Fase 0: Fondasi kecil

| # | Item | Keterangan |
|---|---|---|
| 37 | Perbaiki key `title` vs `window_title` di `AppConfig` | Bug kecil, §4.8 |
| 38 | Perluas `AppConfig`: key string + int baru | `ingest_*`, `prompt_template_path` |
| 39 | `SessionState` + pointer di `MainWindow` | Belum dipakai, hanya wiring |
| 40 | Tambah 3 tombol placeholder di `MainWindow` | Dialog kosong dulu, untuk validasi wiring + CMake |

**Selesai bila**: build hijau di Lubuntu, 11/11 scenario v8 masih PASS (tidak ada regresi).

### Fase A: Prompt Generator (paling kecil, langsung terpakai)

| # | Item | Keterangan |
|---|---|---|
| 41 | `PromptGenerator` (`renderPrompt`) + `prompt_template.txt` default | Isi = prompt Anda saat ini |
| 42 | Test scenario: render dengan/tanpa error, blok kondisional, placeholder tak dikenal | Masuk `frans_scenarios` |
| 43 | `PromptDialog` + tombol **Generate Prompt** + preview + tombol Copy | Tanpa auto-copy / auto-inject |
| 44 | `ManualImportDialog` menulis `last_build_error` ke `SessionState` | Perubahan aditif kecil |
| 45 | Tombol "Ambil dari build terakhir" di `PromptDialog` | Menutup kasus "fix error + output error" |

**Selesai bila**: dari GUI bisa isi permintaan → Generate → hasil identik dengan prompt manual Anda → salin → tempel di browser AI.

### Fase B: Ingest Lokal

| # | Item | Keterangan |
|---|---|---|
| 46 | `RepoIngester`: kumpulkan file via `git ls-files -z` | Jalur utama |
| 47 | Fallback walker `std::filesystem` + exclude bawaan | Untuk folder non-git |
| 48 | Filter: binary, ukuran, secret, total, self-exclude | §4.1 |
| 49 | Renderer format gitingest + ringkasan + daftar skip | Tree + header `FILE:` |
| 50 | Test scenario ingest (temp dir; git opsional → SKIP bila tidak ada) | Masuk `frans_scenarios` |
| 51 | `IngestDialog` + worker thread + Cancel | Pola v8 |
| 52 | Tombol Buka Folder Output (`xdg-open` / `explorer`) | Lewat `ProcessExecutor` |
| 53 | Validasi nyata: ingest repo Frans sendiri, upload ke browser AI, bandingkan dengan hasil gitingest.com | Kriteria sukses §10 |

**Selesai bila**: file `.md` dari Frans bisa dipakai di browser AI setara dengan hasil gitingest.com, dan `.env` terbukti tidak ikut.

### Fase C: Git

| # | Item | Keterangan |
|---|---|---|
| 54 | `ProcessExecutor`: parameter `env_extra` (POSIX) | Backward-compatible |
| 55 | `GitFacade::parseStatusPorcelain` + `suggestCommitMessage` (fungsi murni) + test | Tanpa subprocess, mudah dites |
| 56 | `GitFacade` operasi: status, add, commit, push | Semua argv, `GIT_TERMINAL_PROMPT=0` |
| 57 | Pola git di `DangerCheckHandler` + test | §4.5 |
| 58 | `GitDialog`: status + Add + Commit + Push + konfirmasi push + peringatan file mencurigakan | Konfirmasi di UI thread |
| 59 | Validasi nyata: push ke repo GitHub uji (SSH atau `gh auth`); skenario gagal auth | Pastikan tidak menggantung |
| 60 | `ProcessExecutor_win32.cpp`: env block | ⚠️ ikut tertunda sampai ada akses Windows (v8 #21) |

**Selesai bila**: siklus status → add → commit → push berjalan dari GUI di Lubuntu, dan kegagalan auth muncul sebagai pesan jelas dalam hitungan detik.

### Fase D: Polish (prioritas rendah)

| # | Item | Keterangan |
|---|---|---|
| 61 | Snapshot commit otomatis sebelum Write to Disk (opsional, config) | Rollback murah (v8 #35) |
| 62 | Diff view sebelum Write memakai `DiffEngine` | v8 #27 |
| 63 | Auto-fill paste area dari clipboard setelah menyalin jawaban AI | Pelengkap alur, *bukan* auto-inject ke browser |
| 64 | Daftar recent project roots | v8 #30 |
| 65 | `git push -u origin <branch>` saat belum ada upstream (dengan konfirmasi) | Kemudahan |
| 66 | Dokumen `README.md` + `scripts/README.md` diperbarui | Dokumentasi |

### Ringkasan urutan

```
Fase 0 ──▶ Fase A ──▶ Fase B ──▶ Fase C ──▶ Fase D
(fondasi)  (prompt)   (ingest)    (git)     (polish)
```

Fase A dan B bisa berjalan tanpa menyentuh `ProcessExecutor`. Fase C satu-satunya yang mengubah infrastruktur inti.

---

## 9. Keputusan yang Sudah Diambil

### 9.1 Auto inject

**Keputusan**: **Tidak ada auto-inject maupun auto-copy.** Fasilitas yang disediakan adalah tombol **Generate Prompt**; hasilnya ditampilkan sebagai teks, dan pengguna menyalin + menempelnya sendiri. Tombol Copy manual boleh ada sebagai jalan pintas, tetapi tidak pernah dipicu otomatis.

**Alasan**:
- Aplikasi native tidak bisa menulis ke kotak chat browser secara andal.
- Auto-paste via simulasi keystroke bergantung fokus jendela dan tidak jalan di Wayland.
- Centang auto-inject dibatalkan karena tidak lagi relevan: yang dibutuhkan hanya "generate lalu salin".

### 9.2 Dialog terpisah, bukan tab

**Keputusan**: Ingest / Git / Prompt masing-masing dialog modal, dibuka dari `MainWindow`.

**Alasan**: tidak menyentuh `ManualImportDialog` yang sudah tervalidasi; konsisten dengan pola `show(initial_root)`.

### 9.3 Ingest lokal pakai git, bukan parser `.gitignore` sendiri

**Keputusan**: Jalur utama `git ls-files`; fallback walker dengan exclude bawaan.

**Alasan**: semantik `.gitignore` kompleks; delegasikan ke git. Fallback cukup untuk folder non-git karena targetnya app kecil.

### 9.4 Interpretasi "file yang dikecualikan adalah .gitignore"

**Keputusan**: Dibaca sebagai *"file yang cocok dengan aturan `.gitignore` dikecualikan dari ingest"*. Jika yang dimaksud adalah file `.gitignore` itu sendiri tidak disertakan, cukup tambah satu entri di `extra_excludes` (tidak mengubah desain).

---

## 10. Success Criteria

Frans v3 dianggap sukses kalau:
1. Ingest repo kecil menghasilkan satu `.md` yang bisa di-upload ke browser AI dan setara dengan hasil gitingest.com.
2. Ingest menghormati `.gitignore` penuh (termasuk nested dan negasi) pada repo git.
3. `.env` dan file secret lain tidak pernah masuk ke output ingest dengan pengaturan default.
4. Ingest berjalan tanpa jaringan.
5. Siklus status → add → commit → push berjalan dari GUI di Lubuntu.
6. Push tanpa kredensial gagal cepat dengan pesan jelas (tidak menggantung).
7. Generate Prompt menghasilkan teks identik dengan prompt manual saat ini + permintaan, termasuk error build terakhir bila diminta.
8. Tidak ada regresi: 11/11 scenario v8 tetap PASS; scenario baru PASS.
9. Binary size tetap < 5 MB; startup < 1 detik.
10. Tidak ada AI call, tidak ada dependency baru selain `git` di PATH (opsional; fitur Git/Ingest-jalur-utama menurun dengan anggun bila tidak ada).

---

## 11. Risiko & Mitigasi

| Risiko | Impact | Mitigasi |
|---|---|---|
| Secret ikut ter-upload / ter-commit | High | Exclude default di ingest; peringatan file mencurigakan sebelum `add -A`; output ingest di luar repo |
| Push menggantung menunggu auth | Medium | `GIT_TERMINAL_PROMPT=0` + stdin `/dev/null` + timeout + Cancel |
| `env_extra` di Win32 salah | High (Windows) | Environment block, bukan `SetEnvironmentVariable`; uji di Windows sebelum dipakai produktif (ikut #21 v8) |
| `git` tidak terpasang | Medium | Deteksi di awal (`git --version`); `GitStatus.git_available=false`; ingest jatuh ke fallback walker |
| Hasil ingest terlalu besar untuk konteks AI | Medium | Batas per file + total; tampilkan estimasi token dan daftar skip |
| File ingest di-paste ke Import secara tidak sengaja | Low | Dokumentasikan; baris `FILE:` bisa terbaca parser, tetapi sandbox + preview tetap melindungi |
| Perubahan kecil di `ManualImportDialog` memicu regresi | Medium | Perubahan hanya aditif (pointer opsional); jalankan 11 scenario + uji manual Build/Cancel setelah #44 |
| Parser JSON config minimal tidak cukup | Low | Key tetap flat; upgrade ke nlohmann/json bila berkembang (v8 #33) |
| Pesan commit/argumen dengan karakter khusus | Low | Argv langsung, tanpa shell |

---

## 12. Yang Tidak Akan Dikerjakan (tambahan atas v8 §11)

| Komponen | Alasan |
|---|---|
| Auto-inject / auto-paste ke browser | Rapuh, tidak portabel |
| Ekstensi / userscript browser | Proyek tersendiri |
| Integrasi GitHub API | Cukup `git push` |
| Manajemen branch / merge / rebase / stash | Di luar kebutuhan app kecil |
| Force-push / reset / clean dari UI | Terlalu berisiko; lakukan manual di terminal |
| Parser `.gitignore` custom | Delegasikan ke git |
| Penghitung token akurat per model | Estimasi kasar cukup |
| Chunking ingest otomatis | Target app kecil |

---

## 13. Referensi Cepat (rencana)

```bash
# Build + test (setelah v3 selesai)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFRANS_BUILD_TESTS=ON
cmake --build build -j
./build/bin/frans_scenarios

# Prasyarat git (Linux)
sudo apt install git
gh auth login            # atau siapkan SSH key + ssh-agent

# Prasyarat git (Windows)
# Git for Windows (sudah menyertakan Git Credential Manager)
```

Alur kerja harian yang diharapkan:

1. **Ingest…** → Generate → upload `<repo>-ingest.md` ke browser AI.
2. **Prompt…** → isi permintaan (atau "Ambil dari build terakhir") → Generate → salin → tempel di browser AI.
3. Salin jawaban AI → **Open Import Dialog** → Parse → Write → Build/Test/Run *(v2.1, sudah jadi)*.
4. **Git…** → Refresh → Add All → Commit → Push.
