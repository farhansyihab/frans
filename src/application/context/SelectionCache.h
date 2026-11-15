// src/application/context/SelectionCache.h
//
// Fase E #73 -- Cache hasil seleksi per (hash permintaan + hash daftar file).
//
// Tujuan: hemat panggilan API Gemini. Jika permintaan yang sama diajukan lagi
// terhadap daftar file proyek yang sama, hasil seleksi sebelumnya dipakai ulang
// tanpa memanggil GeminiClient.
//
// Kunci cache  = <hash permintaan> ":" <hash daftar file>
//   - hash permintaan : FNV-1a 64-bit dari field ContextRequest yang
//                       mempengaruhi hasil seleksi (request, error, batas,
//                       model, prompt seleksi, parameter mode).
//                       api_key / endpoint / timeout SENGAJA tidak ikut.
//   - hash daftar file: FNV-1a 64-bit dari (path, size, is_text, skipped)
//                       semua file hasil collectFiles, terurut.
//
// Cache hidup di memori selama aplikasi berjalan (LRU, thread-safe).
#pragma once

#include "application/context/ContextSelector.h"
#include "application/ingest/FileCollector.h"

#include <atomic>
#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace frans::application {

struct SelectionCacheStats {
    std::size_t hits    = 0;
    std::size_t misses  = 0;
    std::size_t entries = 0;
};

// Cache LRU thread-safe untuk SelectionResult.
class SelectionCache {
public:
    // max_entries == 0 berarti cache nonaktif (store() tidak menyimpan apa pun).
    explicit SelectionCache(std::size_t max_entries = 32);

    SelectionCache(const SelectionCache&)            = delete;
    SelectionCache& operator=(const SelectionCache&) = delete;

    // Cari kunci. Jika ada: salin ke `out`, tandai sebagai paling baru, return true.
    bool lookup(const std::string& key, SelectionResult& out);

    // Simpan (atau timpa) hasil. Entri paling lama dibuang bila melebihi batas.
    void store(const std::string& key, const SelectionResult& result);

    void clear();

    std::size_t         size() const;
    std::size_t         maxEntries() const;
    SelectionCacheStats stats() const;

private:
    using Entry = std::pair<std::string, SelectionResult>;

    mutable std::mutex mutex_;
    std::size_t        max_entries_;
    std::list<Entry>   lru_;   // depan = paling baru dipakai
    std::unordered_map<std::string, std::list<Entry>::iterator> index_;
    std::size_t        hits_   = 0;
    std::size_t        misses_ = 0;
};

// ---------------------------------------------------------------------------
// Helper hash (murni, tanpa I/O; diekspos untuk unit test).
// ---------------------------------------------------------------------------

// Hash field ContextRequest yang mempengaruhi hasil seleksi. Return hex 16 karakter.
std::string computeSelectionRequestHash(const ContextRequest& req);

// Hash daftar file (urutan input tidak berpengaruh). Return hex 16 karakter.
std::string computeFileListHash(const std::vector<CollectedFile>& files);

// Gabungkan kedua hash menjadi kunci cache.
std::string makeSelectionCacheKey(const std::string& request_hash,
                                  const std::string& file_list_hash);

// Cache global proses (dipakai oleh overload tanpa parameter cache).
SelectionCache& defaultSelectionCache();

// ---------------------------------------------------------------------------
// Pembungkus selectContext() dengan cache.
// ---------------------------------------------------------------------------

struct SelectionCacheOutcome {
    bool        cache_hit = false;   // true bila hasil berasal dari cache
    std::string key;                 // kunci cache (kosong bila cache dilewati)
};

/**
 * @brief selectContext() dengan cache.
 *
 * Alur:
 *   1. collectFiles (opsi default) -> hash daftar file.
 *   2. Hitung kunci. Bila ada di cache dan !force_refresh -> kembalikan.
 *   3. Bila tidak: selectContext() (panggil Gemini), simpan jika ok dan tidak dibatalkan.
 *
 * Bila collectFiles gagal, cache dilewati dan selectContext() dipanggil langsung
 * sehingga pesan error aslinya tetap muncul.
 */
SelectionResult selectContextCached(const ContextRequest& req,
                                    GeminiClient& client,
                                    SelectionCache& cache,
                                    std::atomic<bool>* cancel = nullptr,
                                    SelectionCacheOutcome* outcome = nullptr,
                                    bool force_refresh = false);

// Overload praktis: memakai cache global. Pengganti langsung selectContext().
SelectionResult selectContextCached(const ContextRequest& req,
                                    GeminiClient& client,
                                    std::atomic<bool>* cancel = nullptr,
                                    SelectionCacheOutcome* outcome = nullptr,
                                    bool force_refresh = false);

} // namespace frans::application