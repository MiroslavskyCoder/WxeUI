#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>

namespace WxeUI {
namespace Cache {

// Многоуровневое кэширование: L1 (GPU), L2 (RAM), L3 (Disk)
enum class CacheLevel {
    L1_GPU = 0,    // GPU память - самая быстрая
    L2_RAM = 1,    // Системная память
    L3_DISK = 2    // Диск с компрессией
};

struct CacheEntry {
    std::vector<uint8_t> data;
    size_t size;
    std::chrono::steady_clock::time_point last_access;
    std::chrono::steady_clock::time_point creation_time;
    uint32_t access_count;
    bool compressed;
    CacheLevel level;
    std::string key;
    
    CacheEntry() : size(0), access_count(0), compressed(false), level(CacheLevel::L2_RAM) {}
};

struct CacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<size_t> total_size{0};
    std::atomic<size_t> entry_count{0};
    std::atomic<uint64_t> evictions{0};
    
    double GetHitRatio() const {
        auto total = hits.load() + misses.load();
        return total > 0 ? static_cast<double>(hits.load()) / total : 0.0;
    }
};

class FragmentCache {
public:
    struct Config {
        size_t max_l1_size = 256 * 1024 * 1024;  // 256MB GPU
        size_t max_l2_size = 1024 * 1024 * 1024; // 1GB RAM
        size_t max_l3_size = 4LL * 1024 * 1024 * 1024; // 4GB Disk
        
        size_t compression_threshold = 64 * 1024; // 64KB
        
        std::chrono::seconds max_age{3600}; // 1 час
        size_t max_entries_per_level = 10000;
        
        bool enable_compression = true;
        bool enable_prefetch = true;
        int compression_level = 6; // zlib уровень
        
        std::string cache_directory = "cache";
    };
    
private:
    struct LRUNode {
        std::string key;
        std::shared_ptr<LRUNode> prev;
        std::shared_ptr<LRUNode> next;
        
        LRUNode(const std::string& k) : key(k) {}
    };
    
    class LRUCache {
    private:
        std::unordered_map<std::string, std::shared_ptr<LRUNode>> cache_map;
        std::shared_ptr<LRUNode> head;
        std::shared_ptr<LRUNode> tail;
        mutable std::mutex mutex_;
        
    public:
        LRUCache();
        ~LRUCache();
        
        void Access(const std::string& key);
        std::string GetLRU();
        void Remove(const std::string& key);
        void Clear();
        size_t Size() const;
        
    private:
        void MoveToHead(std::shared_ptr<LRUNode> node);
        void RemoveNode(std::shared_ptr<LRUNode> node);
    };
    
public:
    explicit FragmentCache(const Config& config = Config{});
    ~FragmentCache();
    
    // Основные операции кэша
    bool Get(const std::string& key, std::vector<uint8_t>& data);
    bool Put(const std::string& key, const std::vector<uint8_t>& data, CacheLevel preferred_level = CacheLevel::L2_RAM);
    bool Remove(const std::string& key);
    void Clear();
    void ClearLevel(CacheLevel level);
    
    // Предзагрузка
    void Prefetch(const std::vector<std::string>& keys);
    void SetPrefetchCallback(std::function<std::vector<uint8_t>(const std::string&)> callback);
    
    // Статистика и мониторинг
    CacheStats GetStats() const;
    void ResetStats();
    size_t GetCurrentSize(CacheLevel level = CacheLevel::L2_RAM) const;
    size_t GetEntryCount(CacheLevel level = CacheLevel::L2_RAM) const;
    
    // Управление памятью
    void Evict(size_t target_size);
    void EvictLevel(CacheLevel level, size_t target_size);
    bool NeedEviction(CacheLevel level) const;
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
    // Thread safety
    void Lock() { main_mutex_.lock(); }
    void Unlock() { main_mutex_.unlock(); }
    
private:
    Config config_;
    
    // Хранилище для каждого уровня
    std::unordered_map<std::string, std::shared_ptr<CacheEntry>> l1_cache_; // GPU
    std::unordered_map<std::string, std::shared_ptr<CacheEntry>> l2_cache_; // RAM
    std::unordered_map<std::string, std::shared_ptr<CacheEntry>> l3_cache_; // Disk
    
    // LRU для каждого уровня
    std::unique_ptr<LRUCache> l1_lru_;
    std::unique_ptr<LRUCache> l2_lru_;
    std::unique_ptr<LRUCache> l3_lru_;
    
    // Статистика
    mutable CacheStats stats_;
    
    // Thread safety
    mutable std::recursive_mutex main_mutex_;
    mutable std::mutex l1_mutex_;
    mutable std::mutex l2_mutex_;
    mutable std::mutex l3_mutex_;
    
    // Компрессия
    std::vector<uint8_t> Compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> Decompress(const std::vector<uint8_t>& compressed_data) const;
    
    // Файловые операции для L3
    bool SaveToFile(const std::string& key, const std::vector<uint8_t>& data);
    bool LoadFromFile(const std::string& key, std::vector<uint8_t>& data);
    std::string GetFilePath(const std::string& key) const;
    
    // Внутренние операции
    bool GetFromLevel(const std::string& key, std::vector<uint8_t>& data, CacheLevel level);
    bool PutToLevel(const std::string& key, const std::vector<uint8_t>& data, CacheLevel level);
    void PromoteEntry(const std::string& key, CacheLevel from, CacheLevel to);
    
    // Управление памятью
    void EvictLRU(CacheLevel level);
    void CleanupExpired();
    bool IsExpired(const CacheEntry& entry) const;
    
    // Предзагрузка
    std::function<std::vector<uint8_t>(const std::string&)> prefetch_callback_;
    std::atomic<bool> prefetch_enabled_{true};
    
    // Фоновые задачи
    void StartBackgroundTasks();
    void StopBackgroundTasks();
    std::atomic<bool> background_running_{false};
    std::thread background_thread_;
    void BackgroundWorker();
};

} // namespace Cache
} // namespace WindowWinapi
