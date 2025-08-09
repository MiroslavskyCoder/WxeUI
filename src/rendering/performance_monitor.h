#pragma once

#include "window_winapi.h"
#include <chrono>
#include <deque>

namespace WxeUI {
namespace rendering {

// Метрики производительности
struct FrameMetrics {
    std::chrono::high_resolution_clock::time_point timestamp;
    float frameTime = 0.0f;      // Общее время кадра
    float cpuTime = 0.0f;        // Время CPU
    float gpuTime = 0.0f;        // Время GPU
    float renderTime = 0.0f;     // Время рендеринга
    size_t drawCalls = 0;        // Количество draw calls
    size_t triangles = 0;        // Количество треугольников
    size_t textureMemory = 0;    // Память текстур
};

// Статистика производительности
struct PerformanceStats {
    // Текущие значения
    float currentFPS = 0.0f;
    float currentFrameTime = 0.0f;
    float currentCpuTime = 0.0f;
    float currentGpuTime = 0.0f;
    
    // Средние значения
    float averageFPS = 0.0f;
    float averageFrameTime = 0.0f;
    float averageCpuTime = 0.0f;
    float averageGpuTime = 0.0f;
    
    // Минимальные/максимальные значения
    float minFPS = FLT_MAX;
    float maxFPS = 0.0f;
    float minFrameTime = FLT_MAX;
    float maxFrameTime = 0.0f;
    
    // Память
    size_t usedMemory = 0;
    size_t totalMemory = 0;
    size_t peakMemory = 0;
    
    // Качество
    size_t frameDrops = 0;
    size_t totalFrames = 0;
    float frameDropRate = 0.0f;
};

// Опции мониторинга
struct MonitoringOptions {
    bool enableFrameProfiling = true;
    bool enableGPUProfiling = true;
    bool enableMemoryTracking = true;
    bool enableHitchDetection = true;
    size_t historySize = 300;  // Количество кадров для статистики
    float hitchThreshold = 33.33f; // Порог для определения просадок
};

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();
    
    // Инициализация
    void Initialize(const MonitoringOptions& options = {});
    void SetOptions(const MonitoringOptions& options) { options_ = options; }
    
    // Основное измерение
    void BeginFrame();
    void EndFrame();
    void BeginGPUWork();
    void EndGPUWork();
    void BeginCPUWork(const std::string& name);
    void EndCPUWork(const std::string& name);
    
    // Трекинг ресурсов
    void TrackDrawCall(size_t triangles = 0);
    void TrackMemoryUsage(size_t textureMemory, size_t bufferMemory = 0);
    void TrackGPUMemoryUsage(size_t used, size_t total);
    
    // Получение статистики
    const PerformanceStats& GetStats() const { return stats_; }
    const std::deque<FrameMetrics>& GetFrameHistory() const { return frameHistory_; }
    
    // FPS управление
    void SetTargetFPS(float targetFPS) { targetFPS_ = targetFPS; }
    float GetTargetFPS() const { return targetFPS_; }
    bool IsFrameRateStable() const;
    
    // Оптимизация
    bool ShouldReduceQuality() const;
    bool ShouldIncreaseQuality() const;
    void ResetStats();
    
    // Отчеты и логирование
    void PrintReport() const;
    void SaveReportToFile(const std::string& filename) const;
    void EnableLogging(bool enable) { loggingEnabled_ = enable; }
    
    // Коллбэки для событий
    std::function<void(const PerformanceStats&)> OnStatsUpdated;
    std::function<void(float)> OnFrameRateChanged;
    std::function<void()> OnPerformanceIssue;
    
private:
    MonitoringOptions options_;
    PerformanceStats stats_;
    std::deque<FrameMetrics> frameHistory_;
    
    // Таймеры
    std::chrono::high_resolution_clock::time_point frameStartTime_;
    std::chrono::high_resolution_clock::time_point gpuStartTime_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> cpuTimers_;
    
    // Подсчет FPS
    float targetFPS_ = 60.0f;
    size_t frameCount_ = 0;
    std::chrono::high_resolution_clock::time_point fpsCounterStart_;
    
    // Логирование
    bool loggingEnabled_ = false;
    
    void UpdateStats();
    void DetectPerformanceIssues();
    float CalculateFrameTime(const FrameMetrics& metrics) const;
};

// Frame Pacing - сглаживание кадров
class FramePacer {
public:
    FramePacer(float targetFPS = 60.0f);
    
    void SetTargetFPS(float fps) { targetFPS_ = fps; frameInterval_ = 1.0f / fps; }
    float GetTargetFPS() const { return targetFPS_; }
    
    // Ожидание следующего кадра
    void WaitForNextFrame();
    
    // VSync управление
    void EnableVSync(bool enable) { vsyncEnabled_ = enable; }
    bool IsVSyncEnabled() const { return vsyncEnabled_; }
    
private:
    float targetFPS_;
    float frameInterval_;
    bool vsyncEnabled_ = true;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
};

}} // namespace window_winapi::rendering
