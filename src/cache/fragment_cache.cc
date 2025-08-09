#include "cache/fragment_cache.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <zlib.h>
#include <thread>

namespace WxeUI {
namespace Cache {

// =============================================================================
// LRUCache Implementation
// =============================================================================

FragmentCache::LRUCache::LRUCache() {
    head = std::make_shared<LRUNode>("");
    tail = std::make_shared<LRUNode>("");
    head->next = tail;
    tail->prev = head;
}

FragmentCache::LRUCache::~LRUCache() {
    Clear();
}

void FragmentCache::LRUCache::Access(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map.find(key);
    if (it != cache_map.end()) {
        MoveToHead(it->second);
    } else {
        auto node = std::make_shared<LRUNode>(key);
        cache_map[key] = node;
        
        // Добавляем в начало
        node->next = head->next;
        node->prev = head;
        head->next->prev = node;
        head->next = node;
    }
}

std::string FragmentCache::LRUCache::GetLRU() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (tail->prev == head) {
        return ""; // Пустой кэш
    }
    
    return tail->prev->key;
}

void FragmentCache::LRUCache::Remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map.find(key);
    if (it != cache_map.end()) {
        RemoveNode(it->second);
        cache_map.erase(it);
    }
}

void FragmentCache::LRUCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_map.clear();
    head->next = tail;
    tail->prev = head;
}

size_t FragmentCache::LRUCache::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map.size();
}

void FragmentCache::LRUCache::MoveToHead(std::shared_ptr<LRUNode> node) {
    // Удаляем узел из текущей позиции
    RemoveNode(node);
    
    // Добавляем в начало
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

void FragmentCache::LRUCache::RemoveNode(std::shared_ptr<LRUNode> node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

// =============================================================================
// FragmentCache Implementation
// =============================================================================

FragmentCache::FragmentCache(const Config& config) 
    : config_(config) {
    l1_lru_ = std::make_unique<LRUCache>();
    l2_lru_ = std::make_unique<LRUCache>();
    l3_lru_ = std::make_unique<LRUCache>();
    
    // Создаем директорию для кэша
    if (!std::filesystem::exists(config_.cache_directory)) {
        std::filesystem::create_directories(config_.cache_directory);
    }
    
    StartBackgroundTasks();
}

FragmentCache::~FragmentCache() {
    StopBackgroundTasks();
}

bool FragmentCache::Get(const std::string& key, std::vector<uint8_t>& data) {
    // Проверяем L1 (GPU)
    if (GetFromLevel(key, data, CacheLevel::L1_GPU)) {
        stats_.hits++;
        l1_lru_->Access(key);
        return true;
    }
    
    // Проверяем L2 (RAM)
    if (GetFromLevel(key, data, CacheLevel::L2_RAM)) {
        stats_.hits++;
        l2_lru_->Access(key);
        // Промотируем в L1 если есть место
        if (GetCurrentSize(CacheLevel::L1_GPU) + data.size() <= config_.max_l1_size) {
            PutToLevel(key, data, CacheLevel::L1_GPU);
        }
        return true;
    }
    
    // Проверяем L3 (Disk)
    if (GetFromLevel(key, data, CacheLevel::L3_DISK)) {
        stats_.hits++;
        l3_lru_->Access(key);
        // Промотируем в L2
        if (GetCurrentSize(CacheLevel::L2_RAM) + data.size() <= config_.max_l2_size) {
            PutToLevel(key, data, CacheLevel::L2_RAM);
        }
        return true;
    }
    
    stats_.misses++;
    return false;
}

bool FragmentCache::Put(const std::string& key, const std::vector<uint8_t>& data, CacheLevel preferred_level) {
    std::lock_guard<std::recursive_mutex> lock(main_mutex_);
    
    // Проверяем, помещается ли в предпочитаемый уровень
    CacheLevel target_level = preferred_level;
    
    if (preferred_level == CacheLevel::L1_GPU && 
        GetCurrentSize(CacheLevel::L1_GPU) + data.size() > config_.max_l1_size) {
        target_level = CacheLevel::L2_RAM;
    }
    
    if (target_level == CacheLevel::L2_RAM && 
        GetCurrentSize(CacheLevel::L2_RAM) + data.size() > config_.max_l2_size) {
        target_level = CacheLevel::L3_DISK;
    }
    
    // Если нужно, очищаем место
    while (NeedEviction(target_level)) {
        EvictLRU(target_level);
    }
    
    return PutToLevel(key, data, target_level);
}

bool FragmentCache::Remove(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lock(main_mutex_);
    
    bool removed = false;
    
    // Удаляем со всех уровней
    {
        std::lock_guard<std::mutex> l1_lock(l1_mutex_);
        auto it = l1_cache_.find(key);
        if (it != l1_cache_.end()) {
            stats_.total_size -= it->second->size;
            l1_cache_.erase(it);
            l1_lru_->Remove(key);
            removed = true;
        }
    }
    
    {
        std::lock_guard<std::mutex> l2_lock(l2_mutex_);
        auto it = l2_cache_.find(key);
        if (it != l2_cache_.end()) {
            stats_.total_size -= it->second->size;
            l2_cache_.erase(it);
            l2_lru_->Remove(key);
            removed = true;
        }
    }
    
    {
        std::lock_guard<std::mutex> l3_lock(l3_mutex_);
        auto it = l3_cache_.find(key);
        if (it != l3_cache_.end()) {
            stats_.total_size -= it->second->size;
            l3_cache_.erase(it);
            l3_lru_->Remove(key);
            
            // Удаляем файл
            std::filesystem::remove(GetFilePath(key));
            removed = true;
        }
    }
    
    if (removed) {
        stats_.entry_count--;
    }
    
    return removed;
}

void FragmentCache::Clear() {
    std::lock_guard<std::recursive_mutex> lock(main_mutex_);
    
    {
        std::lock_guard<std::mutex> l1_lock(l1_mutex_);
        l1_cache_.clear();
        l1_lru_->Clear();
    }
    
    {
        std::lock_guard<std::mutex> l2_lock(l2_mutex_);
        l2_cache_.clear();
        l2_lru_->Clear();
    }
    
    {
        std::lock_guard<std::mutex> l3_lock(l3_mutex_);
        l3_cache_.clear();
        l3_lru_->Clear();
    }
    
    // Очищаем директорию кэша
    try {
        std::filesystem::remove_all(config_.cache_directory);
        std::filesystem::create_directories(config_.cache_directory);
    } catch (...) {
        // Игнорируем ошибки очистки
    }
    
    stats_.total_size = 0;
    stats_.entry_count = 0;
}

void FragmentCache::Prefetch(const std::vector<std::string>& keys) {
    if (!prefetch_enabled_ || !prefetch_callback_) {
        return;
    }
    
    // Асинхронная предзагрузка
    std::thread([this, keys]() {
        for (const auto& key : keys) {
            std::vector<uint8_t> dummy;
            if (!Get(key, dummy)) {
                // Пытаемся загрузить через callback
                try {
                    auto data = prefetch_callback_(key);
                    if (!data.empty()) {
                        Put(key, data);
                    }
                } catch (...) {
                    // Игнорируем ошибки предзагрузки
                }
            }
        }
    }).detach();
}

void FragmentCache::SetPrefetchCallback(std::function<std::vector<uint8_t>(const std::string&)> callback) {
    prefetch_callback_ = callback;
}

CacheStats FragmentCache::GetStats() const {
    return stats_;
}

void FragmentCache::ResetStats() {
    stats_.hits = 0;
    stats_.misses = 0;
    stats_.evictions = 0;
}

size_t FragmentCache::GetCurrentSize(CacheLevel level) const {
    size_t size = 0;
    
    switch (level) {
        case CacheLevel::L1_GPU: {
            std::lock_guard<std::mutex> lock(l1_mutex_);
            for (const auto& pair : l1_cache_) {
                size += pair.second->size;
            }
            break;
        }
        case CacheLevel::L2_RAM: {
            std::lock_guard<std::mutex> lock(l2_mutex_);
            for (const auto& pair : l2_cache_) {
                size += pair.second->size;
            }
            break;
        }
        case CacheLevel::L3_DISK: {
            std::lock_guard<std::mutex> lock(l3_mutex_);
            for (const auto& pair : l3_cache_) {
                size += pair.second->size;
            }
            break;
        }
    }
    
    return size;
}

size_t FragmentCache::GetEntryCount(CacheLevel level) const {
    switch (level) {
        case CacheLevel::L1_GPU: {
            std::lock_guard<std::mutex> lock(l1_mutex_);
            return l1_cache_.size();
        }
        case CacheLevel::L2_RAM: {
            std::lock_guard<std::mutex> lock(l2_mutex_);
            return l2_cache_.size();
        }
        case CacheLevel::L3_DISK: {
            std::lock_guard<std::mutex> lock(l3_mutex_);
            return l3_cache_.size();
        }
        default:
            return 0;
    }
}

void FragmentCache::Evict(size_t target_size) {
    // Реализация вытеснения с учетом целевого размера
    while (stats_.total_size > target_size) {
        // Сначала вытесняем из L1
        if (!l1_cache_.empty()) {
            EvictLRU(CacheLevel::L1_GPU);
        } else if (!l2_cache_.empty()) {
            EvictLRU(CacheLevel::L2_RAM);
        } else if (!l3_cache_.empty()) {
            EvictLRU(CacheLevel::L3_DISK);
        } else {
            break;
        }
    }
}

bool FragmentCache::NeedEviction(CacheLevel level) const {
    size_t current_size = GetCurrentSize(level);
    size_t max_size;
    
    switch (level) {
        case CacheLevel::L1_GPU:
            max_size = config_.max_l1_size;
            break;
        case CacheLevel::L2_RAM:
            max_size = config_.max_l2_size;
            break;
        case CacheLevel::L3_DISK:
            max_size = config_.max_l3_size;
            break;
        default:
            return false;
    }
    
    return current_size >= max_size * 0.9; // 90% заполнения
}

// =============================================================================
// Private Methods
// =============================================================================

bool FragmentCache::GetFromLevel(const std::string& key, std::vector<uint8_t>& data, CacheLevel level) {
    switch (level) {
        case CacheLevel::L1_GPU: {
            std::lock_guard<std::mutex> lock(l1_mutex_);
            auto it = l1_cache_.find(key);
            if (it != l1_cache_.end()) {
                data = it->second->data;
                it->second->last_access = std::chrono::steady_clock::now();
                it->second->access_count++;
                return true;
            }
            break;
        }
        case CacheLevel::L2_RAM: {
            std::lock_guard<std::mutex> lock(l2_mutex_);
            auto it = l2_cache_.find(key);
            if (it != l2_cache_.end()) {
                data = it->second->data;
                it->second->last_access = std::chrono::steady_clock::now();
                it->second->access_count++;
                return true;
            }
            break;
        }
        case CacheLevel::L3_DISK: {
            std::lock_guard<std::mutex> lock(l3_mutex_);
            auto it = l3_cache_.find(key);
            if (it != l3_cache_.end()) {
                if (LoadFromFile(key, data)) {
                    it->second->last_access = std::chrono::steady_clock::now();
                    it->second->access_count++;
                    
                    // Декомпрессия если нужно
                    if (it->second->compressed) {
                        data = Decompress(data);
                    }
                    return true;
                }
            }
            break;
        }
    }
    
    return false;
}

bool FragmentCache::PutToLevel(const std::string& key, const std::vector<uint8_t>& data, CacheLevel level) {
    auto entry = std::make_shared<CacheEntry>();
    entry->key = key;
    entry->level = level;
    entry->creation_time = std::chrono::steady_clock::now();
    entry->last_access = entry->creation_time;
    entry->access_count = 1;
    
    // Компрессия для L3
    if (level == CacheLevel::L3_DISK && config_.enable_compression && 
        data.size() >= config_.compression_threshold) {
        entry->data = Compress(data);
        entry->compressed = true;
    } else {
        entry->data = data;
        entry->compressed = false;
    }
    
    entry->size = entry->data.size();
    
    switch (level) {
        case CacheLevel::L1_GPU: {
            std::lock_guard<std::mutex> lock(l1_mutex_);
            l1_cache_[key] = entry;
            l1_lru_->Access(key);
            break;
        }
        case CacheLevel::L2_RAM: {
            std::lock_guard<std::mutex> lock(l2_mutex_);
            l2_cache_[key] = entry;
            l2_lru_->Access(key);
            break;
        }
        case CacheLevel::L3_DISK: {
            std::lock_guard<std::mutex> lock(l3_mutex_);
            l3_cache_[key] = entry;
            l3_lru_->Access(key);
            
            // Сохраняем на диск
            if (!SaveToFile(key, entry->data)) {
                l3_cache_.erase(key);
                l3_lru_->Remove(key);
                return false;
            }
            break;
        }
        default:
            return false;
    }
    
    stats_.total_size += entry->size;
    stats_.entry_count++;
    
    return true;
}

void FragmentCache::EvictLRU(CacheLevel level) {
    std::string lru_key;
    
    switch (level) {
        case CacheLevel::L1_GPU:
            lru_key = l1_lru_->GetLRU();
            break;
        case CacheLevel::L2_RAM:
            lru_key = l2_lru_->GetLRU();
            break;
        case CacheLevel::L3_DISK:
            lru_key = l3_lru_->GetLRU();
            break;
    }
    
    if (!lru_key.empty()) {
        Remove(lru_key);
        stats_.evictions++;
    }
}

std::vector<uint8_t> FragmentCache::Compress(const std::vector<uint8_t>& data) const {
    uLongf compressed_size = compressBound(data.size());
    std::vector<uint8_t> compressed_data(compressed_size);
    
    int result = compress2(compressed_data.data(), &compressed_size,
                          data.data(), data.size(), config_.compression_level);
    
    if (result == Z_OK) {
        compressed_data.resize(compressed_size);
        return compressed_data;
    }
    
    return data; // Возвращаем исходные данные при ошибке
}

std::vector<uint8_t> FragmentCache::Decompress(const std::vector<uint8_t>& compressed_data) const {
    // Для упрощения предполагаем максимальный размер 10MB
    uLongf decompressed_size = 10 * 1024 * 1024;
    std::vector<uint8_t> decompressed_data(decompressed_size);
    
    int result = uncompress(decompressed_data.data(), &decompressed_size,
                           compressed_data.data(), compressed_data.size());
    
    if (result == Z_OK) {
        decompressed_data.resize(decompressed_size);
        return decompressed_data;
    }
    
    return compressed_data; // Возвращаем сжатые данные при ошибке
}

bool FragmentCache::SaveToFile(const std::string& key, const std::vector<uint8_t>& data) {
    try {
        std::string filepath = GetFilePath(key);
        std::ofstream file(filepath, std::ios::binary);
        if (!file) {
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    } catch (...) {
        return false;
    }
}

bool FragmentCache::LoadFromFile(const std::string& key, std::vector<uint8_t>& data) {
    try {
        std::string filepath = GetFilePath(key);
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file) {
            return false;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        return file.good();
    } catch (...) {
        return false;
    }
}

std::string FragmentCache::GetFilePath(const std::string& key) const {
    // Простое хэширование ключа для имени файла
    std::hash<std::string> hasher;
    size_t hash = hasher(key);
    
    return config_.cache_directory + "/" + std::to_string(hash) + ".cache";
}

void FragmentCache::StartBackgroundTasks() {
    background_running_ = true;
    background_thread_ = std::thread(&FragmentCache::BackgroundWorker, this);
}

void FragmentCache::StopBackgroundTasks() {
    background_running_ = false;
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
}

void FragmentCache::BackgroundWorker() {
    while (background_running_) {
        try {
            CleanupExpired();
            std::this_thread::sleep_for(std::chrono::seconds(30));
        } catch (...) {
            // Игнорируем ошибки фонового потока
        }
    }
}

void FragmentCache::CleanupExpired() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> expired_keys;
    
    // Проверяем L2 кэш
    {
        std::lock_guard<std::mutex> lock(l2_mutex_);
        for (const auto& pair : l2_cache_) {
            if (IsExpired(*pair.second)) {
                expired_keys.push_back(pair.first);
            }
        }
    }
    
    // Проверяем L3 кэш
    {
        std::lock_guard<std::mutex> lock(l3_mutex_);
        for (const auto& pair : l3_cache_) {
            if (IsExpired(*pair.second)) {
                expired_keys.push_back(pair.first);
            }
        }
    }
    
    // Удаляем просроченные записи
    for (const auto& key : expired_keys) {
        Remove(key);
    }
}

bool FragmentCache::IsExpired(const CacheEntry& entry) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.last_access);
    return age > config_.max_age;
}

void FragmentCache::UpdateConfig(const Config& new_config) {
    std::lock_guard<std::recursive_mutex> lock(main_mutex_);
    config_ = new_config;
}

} // namespace Cache
} // namespace WindowWinapi
