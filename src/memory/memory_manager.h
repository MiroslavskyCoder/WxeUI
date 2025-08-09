#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <functional>
#include <condition_variable>

namespace WxeUI {
namespace Memory {

enum class MemoryType {
    SYSTEM_RAM,     // Системная память
    GPU_VRAM,       // Видеопамять
    SHARED_MEMORY,  // Разделяемая память
    MAPPED_MEMORY   // Memory-mapped файлы
};

struct MemoryInfo {
    size_t total_bytes;
    size_t available_bytes;
    size_t used_bytes;
    double usage_percentage;
    
    MemoryInfo() : total_bytes(0), available_bytes(0), used_bytes(0), usage_percentage(0.0) {}
};

struct AllocationStats {
    std::atomic<size_t> total_allocations{0};
    std::atomic<size_t> current_allocations{0};
    std::atomic<size_t> peak_allocations{0};
    std::atomic<size_t> total_bytes_allocated{0};
    std::atomic<size_t> current_bytes_allocated{0};
    std::atomic<size_t> peak_bytes_allocated{0};
    std::atomic<size_t> failed_allocations{0};
    
    void Reset() {
        total_allocations = 0;
        current_allocations = 0;
        peak_allocations = 0;
        total_bytes_allocated = 0;
        current_bytes_allocated = 0;
        peak_bytes_allocated = 0;
        failed_allocations = 0;
    }
};

class MemoryPool {
public:
    struct Config {
        size_t initial_size = 1024 * 1024;  // 1MB
        size_t max_size = 100 * 1024 * 1024; // 100MB
        size_t growth_factor = 2;            // 2x рост
        size_t alignment = 16;               // Выравнивание
        bool auto_shrink = true;             // Автоматическое сжатие
        std::chrono::seconds shrink_timeout{30}; // Время до сжатия
    };
    
private:
    struct Block {
        void* ptr;
        size_t size;
        bool in_use;
        std::chrono::steady_clock::time_point alloc_time;
        std::chrono::steady_clock::time_point free_time;
        
        Block() : ptr(nullptr), size(0), in_use(false) {}
        Block(void* p, size_t s) : ptr(p), size(s), in_use(false) {}
    };
    
public:
    explicit MemoryPool(MemoryType type, const Config& config = Config{});
    ~MemoryPool();
    
    // Основные операции
    void* Allocate(size_t size);
    void* AllocateAligned(size_t size, size_t alignment);
    bool Deallocate(void* ptr);
    void Clear();
    
    // Информация
    size_t GetTotalSize() const;
    size_t GetUsedSize() const;
    size_t GetFreeSize() const;
    size_t GetFragmentation() const; // Процент фрагментации
    
    // Управление размером
    bool Resize(size_t new_size);
    void Shrink();
    void Defragment();
    
    // Статистика
    AllocationStats GetStats() const;
    void ResetStats();
    
private:
    MemoryType type_;
    Config config_;
    
    std::vector<Block> blocks_;
    void* pool_memory_;
    size_t pool_size_;
    size_t used_size_;
    
    mutable std::mutex mutex_;
    AllocationStats stats_;
    
    // Внутренние методы
    void* AllocateInternal(size_t size, size_t alignment);
    bool GrowPool(size_t min_additional_size);
    size_t GetAlignedSize(size_t size, size_t alignment) const;
    void MergeAdjacentBlocks();
};

class MemoryManager {
public:
    struct Config {
        // Пороги памяти (в процентах)
        double warning_threshold = 80.0;    // Предупреждение
        double critical_threshold = 95.0;   // Критический уровень
        double cleanup_threshold = 90.0;    // Начать очистку
        
        // Настройки мониторинга
        std::chrono::milliseconds monitor_interval{1000}; // 1 секунда
        bool enable_auto_cleanup = true;
        bool enable_prediction = true;
        
        // Пулы памяти
        bool enable_memory_pools = true;
        size_t small_pool_size = 16 * 1024 * 1024;   // 16MB для мелких аллокаций
        size_t medium_pool_size = 64 * 1024 * 1024;  // 64MB для средних
        size_t large_pool_size = 256 * 1024 * 1024;  // 256MB для крупных
        
        // Предсказание
        size_t prediction_window = 60; // Окно для предсказания (секунды)
        double growth_prediction_factor = 1.5; // Коэффициент роста
    };
    
    using CleanupCallback = std::function<size_t()>; // Возвращает освобожденные байты
    using WarningCallback = std::function<void(MemoryType, double)>; // Тип, процент использования
    
public:
    explicit MemoryManager(const Config& config = Config{});
    ~MemoryManager();
    
    // Инициализация
    bool Initialize();
    void Shutdown();
    
    // Получение информации о памяти
    MemoryInfo GetMemoryInfo(MemoryType type) const;
    std::unordered_map<MemoryType, MemoryInfo> GetAllMemoryInfo() const;
    
    // Пулы памяти
    void* AllocateFromPool(size_t size, MemoryType type = MemoryType::SYSTEM_RAM);
    bool DeallocateFromPool(void* ptr, MemoryType type = MemoryType::SYSTEM_RAM);
    
    // Управление памятью
    size_t FreeUnusedMemory(); // Освобождает неиспользуемую память
    size_t ForceCleanup();     // Принудительная очистка
    bool RequestMemory(size_t size, MemoryType type); // Запрос выделения памяти
    
    // Мониторинг
    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const;
    
    // Callbacks
    void RegisterCleanupCallback(const std::string& name, CleanupCallback callback);
    void UnregisterCleanupCallback(const std::string& name);
    void SetWarningCallback(WarningCallback callback);
    
    // Предсказание потребностей
    size_t PredictMemoryUsage(MemoryType type, std::chrono::seconds future_time) const;
    bool WillExceedThreshold(MemoryType type, std::chrono::seconds future_time, double threshold) const;
    
    // Статистика
    AllocationStats GetPoolStats(MemoryType type) const;
    void ResetAllStats();
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
private:
    Config config_;
    
    // Пулы для разных типов памяти
    std::unordered_map<MemoryType, std::unique_ptr<MemoryPool>> memory_pools_;
    
    // Callbacks
    std::unordered_map<std::string, CleanupCallback> cleanup_callbacks_;
    WarningCallback warning_callback_;
    
    // Мониторинг
    std::atomic<bool> monitoring_active_{false};
    std::thread monitor_thread_;
    mutable std::mutex monitor_mutex_;
    
    // Предсказание
    struct MemoryUsagePoint {
        std::chrono::steady_clock::time_point timestamp;
        size_t used_bytes;
    };
    
    std::unordered_map<MemoryType, std::vector<MemoryUsagePoint>> usage_history_;
    mutable std::mutex history_mutex_;
    
    // Platform-specific данные
#ifdef _WIN32
    struct Win32MemoryData;
    std::unique_ptr<Win32MemoryData> win32_data_;
#endif
    
    // Внутренние методы
    bool InitializePlatform();
    void ShutdownPlatform();
    
    void MonitorThread();
    void CheckMemoryLevels();
    void ExecuteCleanup();
    
    // Получение информации о памяти (platform-specific)
    MemoryInfo GetSystemMemoryInfo() const;
    MemoryInfo GetGPUMemoryInfo() const;
    
    // Предсказание
    void RecordMemoryUsage(MemoryType type, size_t used_bytes);
    double CalculateGrowthRate(MemoryType type) const;
    
    // Утилиты
    MemoryType GetOptimalPoolType(size_t size) const;
    void LogMemoryWarning(MemoryType type, double usage_percent) const;
};

// Глобальный менеджер памяти
class GlobalMemoryManager {
public:
    static MemoryManager& Instance();
    static void Initialize(const MemoryManager::Config& config = MemoryManager::Config{});
    static void Shutdown();
    
    // Convenience functions
    static void* Allocate(size_t size, MemoryType type = MemoryType::SYSTEM_RAM);
    static void Deallocate(void* ptr, MemoryType type = MemoryType::SYSTEM_RAM);
    static MemoryInfo GetMemoryInfo(MemoryType type);
    
private:
    static std::unique_ptr<MemoryManager> instance_;
    static std::mutex instance_mutex_;
};

} // namespace Memory
} // namespace WindowWinapi
