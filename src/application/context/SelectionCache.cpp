// src/application/context/SelectionCache.cpp
//
// Fase E #73 -- implementasi cache seleksi.
#include "application/context/SelectionCache.h"
#include "application/context/SourceCodeBuilder.h"   // fnv1a_64_hex

#include <algorithm>

namespace frans::application {

namespace {

// Tambahkan field dengan prefix panjang agar tidak ambigu
// (mis. ["ab","c"] berbeda dari ["a","bc"]).
void appendField(std::string& out, const std::string& value)
{
    out += std::to_string(value.size());
    out += ':';
    out += value;
    out += '\n';
}

void appendNumber(std::string& out, std::size_t value)
{
    appendField(out, std::to_string(value));
}

} // namespace

// ---------------------------------------------------------------------------
// SelectionCache
// ---------------------------------------------------------------------------

SelectionCache::SelectionCache(std::size_t max_entries)
    : max_entries_(max_entries)
{
}

bool SelectionCache::lookup(const std::string& key, SelectionResult& out)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = index_.find(key);
    if (it == index_.end()) {
        ++misses_;
        return false;
    }

    // Pindahkan ke depan (paling baru dipakai).
    lru_.splice(lru_.begin(), lru_, it->second);
    out = it->second->second;
    ++hits_;
    return true;
}

void SelectionCache::store(const std::string& key, const SelectionResult& result)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (max_entries_ == 0) return;

    auto it = index_.find(key);
    if (it != index_.end()) {
        it->second->second = result;
        lru_.splice(lru_.begin(), lru_, it->second);
        return;
    }

    lru_.emplace_front(key, result);
    index_[key] = lru_.begin();

    while (lru_.size() > max_entries_) {
        index_.erase(lru_.back().first);
        lru_.pop_back();
    }
}

void SelectionCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    lru_.clear();
    index_.clear();
}

std::size_t SelectionCache::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_.size();
}

std::size_t SelectionCache::maxEntries() const
{
    return max_entries_;
}

SelectionCacheStats SelectionCache::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    SelectionCacheStats s;
    s.hits    = hits_;
    s.misses  = misses_;
    s.entries = lru_.size();
    return s;
}

// ---------------------------------------------------------------------------
// Hash helpers
// ---------------------------------------------------------------------------

std::string computeSelectionRequestHash(const ContextRequest& req)
{
    std::string buf;
    appendField(buf, "sel-cache-v1");   // versi skema kunci

    appendField(buf, req.project_root);
    appendField(buf, req.request_text);
    appendField(buf, req.error_output);

    appendNumber(buf, req.always_include.size());
    for (const auto& p : req.always_include) appendField(buf, p);

    appendNumber(buf, req.max_context_bytes);
    appendNumber(buf, req.max_files);

    appendField(buf, req.gemini_model);
    appendField(buf, req.selector_prompt);
    appendNumber(buf, req.gemini_full_mode_max_tokens);
    appendNumber(buf, static_cast<std::size_t>(
        req.gemini_index_head_lines < 0 ? 0 : req.gemini_index_head_lines));

    // api_key, gemini_endpoint, gemini_timeout_sec sengaja TIDAK ikut hash:
    // tidak mengubah hasil seleksi, dan key tidak boleh masuk ke apa pun.
    return fnv1a_64_hex(buf);
}

std::string computeFileListHash(const std::vector<CollectedFile>& files)
{
    // Urutkan salinan berdasarkan path supaya hash tidak bergantung urutan input.
    std::vector<const CollectedFile*> sorted;
    sorted.reserve(files.size());
    for (const auto& f : files) sorted.push_back(&f);

    std::sort(sorted.begin(), sorted.end(),
              [](const CollectedFile* a, const CollectedFile* b) {
                  return a->relative_path < b->relative_path;
              });

    std::string buf;
    appendField(buf, "file-list-v1");
    appendNumber(buf, sorted.size());
    for (const CollectedFile* f : sorted) {
        appendField(buf, f->relative_path);
        appendNumber(buf, f->size_bytes);
        appendField(buf, f->is_text ? "t" : "b");
        appendField(buf, f->skipped ? "s" : "r");
    }
    return fnv1a_64_hex(buf);
}

std::string makeSelectionCacheKey(const std::string& request_hash,
                                  const std::string& file_list_hash)
{
    return request_hash + ":" + file_list_hash;
}

SelectionCache& defaultSelectionCache()
{
    static SelectionCache cache(32);
    return cache;
}

// ---------------------------------------------------------------------------
// selectContextCached
// ---------------------------------------------------------------------------

SelectionResult selectContextCached(const ContextRequest& req,
                                    GeminiClient& client,
                                    SelectionCache& cache,
                                    std::atomic<bool>* cancel,
                                    SelectionCacheOutcome* outcome,
                                    bool force_refresh)
{
    if (outcome) *outcome = SelectionCacheOutcome{};

    // 1. Daftar file nyata (murni lokal, tidak memanggil API).
    CollectOptions opt;   // opsi default; hanya dipakai untuk kunci cache
    CollectResult collected = collectFiles(req.project_root, opt, cancel);
    if (!collected.ok) {
        // Biarkan selectContext melaporkan error aslinya; jangan cache.
        return selectContext(req, client, cancel);
    }

    // 2. Kunci cache.
    const std::string key = makeSelectionCacheKey(
        computeSelectionRequestHash(req),
        computeFileListHash(collected.files));
    if (outcome) outcome->key = key;

    if (!force_refresh) {
        SelectionResult cached;
        if (cache.lookup(key, cached)) {
            if (outcome) outcome->cache_hit = true;
            return cached;
        }
    }

    // 3. Miss: panggil Gemini.
    SelectionResult result = selectContext(req, client, cancel);

    const bool cancelled = cancel && cancel->load();
    if (result.ok && !cancelled) {
        cache.store(key, result);
    }
    return result;
}

SelectionResult selectContextCached(const ContextRequest& req,
                                    GeminiClient& client,
                                    std::atomic<bool>* cancel,
                                    SelectionCacheOutcome* outcome,
                                    bool force_refresh)
{
    return selectContextCached(req, client, defaultSelectionCache(),
                               cancel, outcome, force_refresh);
}

} // namespace frans::application