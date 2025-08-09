#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <thread>
#include <fstream>
#include <shared_mutex>

namespace WxeUI {
namespace Profiling {

enum class CacheEventType {
    HIT,            // Попадание в кэш
    MISS,           // Промах кэша
    EVICTION,       // Вытеснение из кэша
    INSERTION,      // Добавление в кэш
    UPDATE,         // Обновление записи
    COMPRESSION,    // Сжатие данных
    DECOMPRESSION,  // Распаковка данных
    GPU_UPLOAD,     // Загрузка в GPU
    GPU_EVICTION,   // Выгрузка из GPU
    CLEANUP         // Очистка кэша
};

struct CacheEvent {
    CacheEventType type;
    std::string cache_name;         // Имя кэша (fragment, texture, shader)
    std::string key;                // Ключ записи
    size_t data_size;               // Размер данных в байтах
    std::chrono::steady_clock::time_point timestamp;
    double duration_ms;             // Длительность операции в миллисекундах
    std::string additional_info;    // Дополнительная информация
    
    CacheEvent() : type(CacheEventType::HIT), data_size(0), duration_ms(0.0) {
        timestamp = std::chrono::steady_clock::now();
    }
};

struct CacheMetrics {
    // Базовые метрики
    std::atomic<uint64_t> total_hits{0};
    std::atomic<uint64_t> total_misses{0};
    std::atomic<uint64_t> total_evictions{0};
    std::atomic<uint64_t> total_insertions{0};
    
    // Производительность
    std::atomic<double> avg_hit_time_ms{0.0};
    std::atomic<double> avg_miss_time_ms{0.0};
    std::atomic<double> avg_eviction_time_ms{0.0};
    std::atomic<double> avg_insertion_time_ms{0.0};
    
    // Память
    std::atomic<size_t> peak_memory_usage{0};
    std::atomic<size_t> current_memory_usage{0};
    std::atomic<size_t> total_bytes_processed{0};
    
    // Эффективность
    double GetHitRatio() const {
        auto total = total_hits.load() + total_misses.load();
        return total > 0 ? static_cast<double>(total_hits.load()) / total : 0.0;
    }
    
    double GetEvictionRate() const {
        auto total = total_insertions.load();
        return total > 0 ? static_cast<double>(total_evictions.load()) / total : 0.0;
    }
};

struct AccessPattern {
    std::string key;
    std::vector<std::chrono::steady_clock::time_point> access_times;
    size_t total_accesses;
    double avg_interval_ms;         // Средний интервал между обращениями
    bool is_hot;                    // "Горячая" запись (часто используется)
    bool is_temporal;               // Временная зависимость
    bool is_sequential;             // Последовательный доступ
    
    AccessPattern() : total_accesses(0), avg_interval_ms(0.0), 
                    is_hot(false), is_temporal(false), is_sequential(false) {}
};

struct PerformanceSnapshot {
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, CacheMetrics> cache_metrics; // По каждому кэшу
    
    // Системные метрики
    size_t system_memory_used;
    size_t gpu_memory_used;
    double cpu_usage;
    double gpu_usage;
    
    // Агрегированные метрики
    double overall_hit_ratio;
    size_t total_cache_size;
    size_t total_entries;
    
    PerformanceSnapshot() : system_memory_used(0), gpu_memory_used(0), 
                        cpu_usage(0.0), gpu_usage(0.0), 
                        overall_hit_ratio(0.0), total_cache_size(0), total_entries(0) {
        timestamp = std::chrono::steady_clock::now();
    }
};

class CacheProfiler {
public:
    struct Config {
        Config() {}

        // Включение/выключение функций
        bool enable_profiling = true;
        bool enable_event_logging = true;
        bool enable_pattern_analysis = true;
        bool enable_performance_snapshots = true;
        
        // Сбор данных
        size_t max_events = 100000;                     // Максимум событий в памяти
        size_t max_patterns = 10000;                    // Максимум паттернов
        std::chrono::seconds snapshot_interval{10};     // Интервал снимков
        std::chrono::seconds pattern_update_interval{30}; // Обновление паттернов
        
        // Анализ паттернов
        size_t hot_access_threshold = 10;               // Порог для "горячих" записей
        std::chrono::seconds temporal_window{60};       // Окно для временного анализа
        double sequential_threshold = 0.8;              // Порог для последовательного доступа
        
        // Вывод
        bool save_to_file = true;
        std::string log_directory = "cache_logs";
        bool compress_logs = true;
        
        // Визуализация
        bool generate_reports = true;
        bool generate_charts = false;                   // Требует дополнительных библиотек
        std::chrono::seconds report_interval{300};      // 5 минут
    };
    
    using EventCallback = std::function<void(const CacheEvent&)>;
    using PatternCallback = std::function<void(const AccessPattern&)>;
    
public:
    explicit CacheProfiler(const Config& config = Config{});
    ~CacheProfiler();
    
    // Инициализация
    bool Initialize();
    void Shutdown();
    
    // Регистрация событий
    void RecordEvent(CacheEventType type, const std::string& cache_name, 
                    const std::string& key, size_t data_size = 0, 
                    double duration_ms = 0.0, const std::string& info = "");
    
    void RecordHit(const std::string& cache_name, const std::string& key, 
                size_t data_size = 0, double duration_ms = 0.0);
    
    void RecordMiss(const std::string& cache_name, const std::string& key,
                double duration_ms = 0.0);
    
    void RecordEviction(const std::string& cache_name, const std::string& key,
                size_t data_size = 0, double duration_ms = 0.0);
    
    // Метрики по кэшам
    void UpdateCacheMetrics(const std::string& cache_name, const CacheMetrics& metrics);
    CacheMetrics GetCacheMetrics(const std::string& cache_name) const;
    std::unordered_map<std::string, CacheMetrics> GetAllCacheMetrics() const;
    
    // Анализ паттернов
    void AnalyzeAccessPatterns();
    std::vector<AccessPattern> GetHotPatterns() const;
    std::vector<AccessPattern> GetTemporalPatterns() const;
    std::vector<AccessPattern> GetSequentialPatterns() const;
    
    // Снимки производительности
    PerformanceSnapshot TakeSnapshot();
    std::vector<PerformanceSnapshot> GetSnapshotHistory() const;
    void ClearSnapshotHistory();
    
    // Отчеты и экспорт
    void GenerateReport(const std::string& filename = "");
    void ExportEvents(const std::string& filename, 
                    std::chrono::steady_clock::time_point start = {},
                    std::chrono::steady_clock::time_point end = {});
    
    void ExportMetrics(const std::string& filename);
    void ExportPatterns(const std::string& filename);
    
    // Рекомендации по оптимизации
    struct OptimizationRecommendation {
        std::string cache_name;
        std::string recommendation_type;
        std::string description;
        double expected_improvement;    // Ожидаемое улучшение в процентах
        int priority;                   // 1-10, где 10 - высший приоритет
    };
    
    std::vector<OptimizationRecommendation> GetOptimizationRecommendations() const;
    
    // Callbacks
    void SetEventCallback(EventCallback callback);
    void SetPatternCallback(PatternCallback callback);
    
    // Управление
    void StartProfiling();
    void StopProfiling();
    bool IsProfiling() const;
    
    void ClearData();
    void ResetMetrics();
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
    // Утилиты
    static std::string GetEventTypeName(CacheEventType type);
    static std::string FormatDuration(double duration_ms);
    static std::string FormatSize(size_t bytes);
    
private:
    Config config_;
    
    // Состояние
    std::atomic<bool> profiling_active_{false};
    std::atomic<bool> initialized_{false};
    
    // События
    std::vector<CacheEvent> events_;
    mutable std::mutex events_mutex_;
    
    // Метрики
    std::unordered_map<std::string, CacheMetrics> cache_metrics_;
    mutable std::shared_mutex metrics_mutex_;
    
    // Паттерны доступа
    std::unordered_map<std::string, AccessPattern> access_patterns_;
    mutable std::mutex patterns_mutex_;
    
    // Снимки производительности
    std::vector<PerformanceSnapshot> snapshots_;
    mutable std::mutex snapshots_mutex_;
    
    // Callbacks
    EventCallback event_callback_;
    PatternCallback pattern_callback_;
    
    // Фоновые потоки
    std::thread snapshot_thread_;
    std::thread pattern_analysis_thread_;
    std::thread report_thread_;
    std::atomic<bool> threads_running_{false};
    
    // Файловый вывод
    std::unique_ptr<std::ofstream> log_file_;
    mutable std::mutex log_mutex_;
    
    // Внутренние методы
    void SnapshotWorker();
    void PatternAnalysisWorker();
    void ReportWorker();
    
    // Анализ
    void UpdateAccessPattern(const std::string& key);
    void AnalyzePattern(AccessPattern& pattern);
    bool IsSequentialAccess(const std::vector<std::string>& keys) const;
    double CalculateAccessInterval(const AccessPattern& pattern) const;
    
    // Оптимизация
    std::vector<OptimizationRecommendation> AnalyzeCachePerformance() const;
    OptimizationRecommendation AnalyzeHitRatio(const std::string& cache_name, 
                                            const CacheMetrics& metrics) const;
    OptimizationRecommendation AnalyzeEvictionRate(const std::string& cache_name,
                                            const CacheMetrics& metrics) const;
    OptimizationRecommendation AnalyzeMemoryUsage(const std::string& cache_name,
                                            const CacheMetrics& metrics) const;
    
    // Вывод и форматирование
    void WriteEventToLog(const CacheEvent& event);
    std::string FormatEvent(const CacheEvent& event) const;
    std::string FormatMetrics(const CacheMetrics& metrics) const;
    
    // Системные метрики
    size_t GetSystemMemoryUsage() const;
    size_t GetGPUMemoryUsage() const;
    double GetCPUUsage() const;
    double GetGPUUsage() const;
    
    // Утилиты
    void EnsureDirectoryExists(const std::string& path);
    std::string GetLogFileName() const;
    std::string GetReportFileName() const;
    void CompressFile(const std::string& filename);
};

// Глобальный профайлер кэша
class GlobalCacheProfiler {
public:
    static CacheProfiler& Instance();
    static void Initialize(const CacheProfiler::Config& config = CacheProfiler::Config{});
    static void Shutdown();
    
    // Convenience functions
    static void RecordHit(const std::string& cache_name, const std::string& key,
                        size_t data_size = 0, double duration_ms = 0.0);
    static void RecordMiss(const std::string& cache_name, const std::string& key,
                        double duration_ms = 0.0);
    static void RecordEviction(const std::string& cache_name, const std::string& key,
                        size_t data_size = 0, double duration_ms = 0.0);
    
private:
    static std::unique_ptr<CacheProfiler> instance_;
    static std::mutex instance_mutex_;
};

} // namespace Profiling
} // namespace WindowWinapi
