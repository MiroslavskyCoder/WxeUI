#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>

namespace WxeUI {
namespace Cache {

enum class TextureFormat {
    RGBA8,          // 32-bit RGBA
    BGRA8,          // 32-bit BGRA
    RGB8,           // 24-bit RGB
    RGBA16F,        // 64-bit HDR RGBA
    RGBA32F,        // 128-bit HDR RGBA
    BC1,            // DXT1 compression
    BC3,            // DXT5 compression
    BC7,            // High-quality compression
    ASTC_4x4,       // ASTC 4x4 block
    ASTC_8x8,       // ASTC 8x8 block
    ETC2_RGB,       // ETC2 RGB
    ETC2_RGBA       // ETC2 RGBA
};

enum class TextureQuality {
    LOW = 0,        // Минимальное качество
    MEDIUM = 1,     // Среднее качество
    HIGH = 2,       // Высокое качество
    ULTRA = 3       // Максимальное качество
};

struct TextureDescriptor {
    uint32_t width;
    uint32_t height;
    uint32_t depth;         // 1 для 2D, >1 для 3D
    uint32_t mip_levels;
    uint32_t array_size;    // Количество слоев в массиве
    TextureFormat format;
    TextureQuality quality;
    bool generate_mipmaps;
    bool compress;
    
    TextureDescriptor() : width(0), height(0), depth(1), mip_levels(1), 
                         array_size(1), format(TextureFormat::RGBA8), 
                         quality(TextureQuality::MEDIUM), 
                         generate_mipmaps(false), compress(false) {}
    
    // Генерация ключа для кэша
    std::string GetCacheKey() const;
    size_t GetDataSize() const;
    bool IsCompressed() const;
};

struct TextureData {
    std::vector<uint8_t> data;
    TextureDescriptor descriptor;
    size_t size;
    std::chrono::steady_clock::time_point creation_time;
    std::chrono::steady_clock::time_point last_access;
    uint32_t access_count;
    bool is_resident;       // Находится ли в GPU памяти
    void* gpu_handle;       // Platform-specific GPU handle
    
    TextureData() : size(0), access_count(0), is_resident(false), gpu_handle(nullptr) {
        creation_time = std::chrono::steady_clock::now();
        last_access = creation_time;
    }
};

struct TextureCacheStats {
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> textures_loaded{0};
    std::atomic<uint64_t> textures_generated{0};
    std::atomic<uint64_t> compressions_performed{0};
    std::atomic<uint64_t> mipmaps_generated{0};
    std::atomic<size_t> total_memory_used{0};
    std::atomic<size_t> gpu_memory_used{0};
    
    double GetHitRatio() const {
        auto total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }
};

class TextureCache {
public:
    struct Config {
        size_t max_cache_size = 512 * 1024 * 1024;     // 512MB общий лимит
        size_t max_gpu_memory = 256 * 1024 * 1024;      // 256MB GPU памяти
        size_t max_entries = 10000;                     // Макс количество текстур
        
        // Предзагрузка
        bool enable_preloading = true;
        size_t preload_queue_size = 100;
        uint32_t preload_threads = 2;
        
        // Компрессия
        bool auto_compress = true;
        size_t compression_threshold = 1024 * 1024;     // 1MB
        TextureFormat preferred_compression = TextureFormat::BC7;
        
        // Mipmaps
        bool auto_generate_mipmaps = true;
        uint32_t max_mip_levels = 12;                   // до 4096x4096
        
        // Качество по умолчанию
        TextureQuality default_quality = TextureQuality::MEDIUM;
        
        // Очистка
        std::chrono::seconds max_unused_time{300};      // 5 минут
        double cleanup_threshold = 0.9;                 // 90% заполнения
        
        std::string cache_directory = "texture_cache";
    };
    
public:
    explicit TextureCache(const Config& config = Config{});
    ~TextureCache();
    
    // Инициализация
    bool Initialize();
    void Shutdown();
    
    // Основные операции
    std::shared_ptr<TextureData> GetTexture(const std::string& key);
    std::shared_ptr<TextureData> GetTexture(const std::string& key, const TextureDescriptor& descriptor);
    
    bool StoreTexture(const std::string& key, const std::vector<uint8_t>& data, const TextureDescriptor& descriptor);
    bool StoreTexture(const std::string& key, std::shared_ptr<TextureData> texture);
    
    bool RemoveTexture(const std::string& key);
    void ClearCache();
    
    // Генерация текстур
    std::shared_ptr<TextureData> CreateTexture(const TextureDescriptor& descriptor);
    std::shared_ptr<TextureData> LoadTexture(const std::string& filepath);
    std::shared_ptr<TextureData> LoadTexture(const std::string& filepath, const TextureDescriptor& descriptor);
    
    // Mipmaps
    bool GenerateMipmaps(std::shared_ptr<TextureData> texture);
    std::vector<uint8_t> GenerateMipLevel(const std::vector<uint8_t>& src_data, 
                                         uint32_t src_width, uint32_t src_height,
                                         uint32_t dst_width, uint32_t dst_height,
                                         TextureFormat format);
    
    // Компрессия
    std::vector<uint8_t> CompressTexture(const std::vector<uint8_t>& data, 
                                        const TextureDescriptor& src_desc,
                                        TextureFormat target_format);
    
    std::vector<uint8_t> DecompressTexture(const std::vector<uint8_t>& compressed_data,
                                          const TextureDescriptor& desc);
    
    // Предзагрузка
    void PreloadTextures(const std::vector<std::string>& texture_keys);
    void PreloadTexture(const std::string& key);
    bool IsPreloading() const;
    
    // GPU управление
    bool PromoteToGPU(const std::string& key);
    bool EvictFromGPU(const std::string& key);
    void OptimizeGPUMemory();
    
    // Статистика
    TextureCacheStats GetStats() const;
    void ResetStats();
    size_t GetCacheSize() const;
    size_t GetGPUMemoryUsage() const;
    size_t GetEntryCount() const;
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
    // Утилиты
    static uint32_t GetBytesPerPixel(TextureFormat format);
    static bool IsCompressedFormat(TextureFormat format);
    static std::string GetFormatName(TextureFormat format);
    static std::vector<TextureFormat> GetSupportedFormats();
    static uint32_t CalculateMipLevels(uint32_t width, uint32_t height);
    
private:
    Config config_;
    
    // Хранилище текстур
    std::unordered_map<std::string, std::shared_ptr<TextureData>> cache_;
    mutable std::shared_mutex cache_mutex_;
    
    // Статистика
    mutable TextureCacheStats stats_;
    
    // Предзагрузка
    std::queue<std::string> preload_queue_;
    std::vector<std::thread> preload_threads_;
    std::atomic<bool> preloading_active_{false};
    std::mutex preload_mutex_;
    std::condition_variable preload_cv_;
    
    // Platform-specific данные
#ifdef _WIN32
    struct Win32TextureData;
    std::unique_ptr<Win32TextureData> win32_data_;
#endif
    
    // Внутренние методы
    bool InitializePlatform();
    void ShutdownPlatform();
    
    void PreloadWorker();
    void CleanupUnused();
    void EvictLRU();
    
    // Компрессия
    std::vector<uint8_t> CompressBC1(const std::vector<uint8_t>& data, uint32_t width, uint32_t height);
    std::vector<uint8_t> CompressBC3(const std::vector<uint8_t>& data, uint32_t width, uint32_t height);
    std::vector<uint8_t> CompressBC7(const std::vector<uint8_t>& data, uint32_t width, uint32_t height);
    std::vector<uint8_t> CompressASTC(const std::vector<uint8_t>& data, uint32_t width, uint32_t height, uint32_t block_size);
    
    // GPU операции
    void* CreateGPUTexture(const TextureData& texture);
    void DestroyGPUTexture(void* gpu_handle);
    bool UploadToGPU(std::shared_ptr<TextureData> texture);
    
    // Файловые операции
    bool SaveToFile(const std::string& key, const TextureData& texture);
    bool LoadFromFile(const std::string& key, std::shared_ptr<TextureData>& texture);
    std::string GetCacheFilePath(const std::string& key) const;
    
    // Оптимизация
    bool ShouldCompress(const TextureDescriptor& descriptor) const;
    TextureFormat GetOptimalCompressionFormat(const TextureDescriptor& descriptor) const;
    bool NeedsEviction() const;
    
    // Утилиты
    void UpdateAccessTime(std::shared_ptr<TextureData> texture);
    size_t CalculateTextureSize(const TextureDescriptor& descriptor) const;
};

} // namespace Cache
} // namespace WindowWinapi
