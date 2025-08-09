#include "rendering/performance_monitor.h"
#include <fstream>
#include <thread>
#include <algorithm>
#include <iostream>

namespace WxeUI {
namespace rendering {

PerformanceMonitor::PerformanceMonitor() {
    fpsCounterStart_ = std::chrono::high_resolution_clock::now();
}

PerformanceMonitor::~PerformanceMonitor() {
}

void PerformanceMonitor::Initialize(const MonitoringOptions& options) {
    options_ = options;
    frameHistory_.clear();
    ResetStats();
}

void PerformanceMonitor::BeginFrame() {
    frameStartTime_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::EndFrame() {
    auto now = std::chrono::high_resolution_clock::now();
    
    FrameMetrics metrics;
    metrics.timestamp = now;
    metrics.frameTime = std::chrono::duration<float, std::milli>(now - frameStartTime_).count();
    
    // Добавляем метрики в историю
    frameHistory_.push_back(metrics);
    
    // Ограничиваем размер истории
    if (frameHistory_.size() > options_.historySize) {
        frameHistory_.pop_front();
    }
    
    frameCount_++;
    UpdateStats();
    
    // Проверяем производительность
    if (options_.enableHitchDetection) {
        DetectPerformanceIssues();
    }
    
    // Уведомляем о обновлении статистики
    if (OnStatsUpdated) {
        OnStatsUpdated(stats_);
    }
}

void PerformanceMonitor::BeginGPUWork() {
    if (options_.enableGPUProfiling) {
        gpuStartTime_ = std::chrono::high_resolution_clock::now();
    }
}

void PerformanceMonitor::EndGPUWork() {
    if (options_.enableGPUProfiling && !frameHistory_.empty()) {
        auto now = std::chrono::high_resolution_clock::now();
        frameHistory_.back().gpuTime = 
            std::chrono::duration<float, std::milli>(now - gpuStartTime_).count();
    }
}

void PerformanceMonitor::BeginCPUWork(const std::string& name) {
    if (options_.enableFrameProfiling) {
        cpuTimers_[name] = std::chrono::high_resolution_clock::now();
    }
}

void PerformanceMonitor::EndCPUWork(const std::string& name) {
    if (options_.enableFrameProfiling) {
        auto it = cpuTimers_.find(name);
        if (it != cpuTimers_.end() && !frameHistory_.empty()) {
            auto now = std::chrono::high_resolution_clock::now();
            float workTime = std::chrono::duration<float, std::milli>(now - it->second).count();
            frameHistory_.back().cpuTime += workTime;
            cpuTimers_.erase(it);
        }
    }
}

void PerformanceMonitor::TrackDrawCall(size_t triangles) {
    if (!frameHistory_.empty()) {
        frameHistory_.back().drawCalls++;
        frameHistory_.back().triangles += triangles;
    }
}

void PerformanceMonitor::TrackMemoryUsage(size_t textureMemory, size_t bufferMemory) {
    if (!frameHistory_.empty()) {
        frameHistory_.back().textureMemory = textureMemory;
    }
    
    stats_.usedMemory = textureMemory + bufferMemory;
    if (stats_.usedMemory > stats_.peakMemory) {
        stats_.peakMemory = stats_.usedMemory;
    }
}

void PerformanceMonitor::TrackGPUMemoryUsage(size_t used, size_t total) {
    stats_.usedMemory = used;
    stats_.totalMemory = total;
}

bool PerformanceMonitor::IsFrameRateStable() const {
    if (frameHistory_.size() < 30) return false; // Недостаточно данных
    
    // Проверяем стабильность за последние 30 кадров
    float sum = 0.0f;
    float sumSquares = 0.0f;
    size_t count = std::min(frameHistory_.size(), size_t(30));
    
    auto it = frameHistory_.end() - count;
    for (; it != frameHistory_.end(); ++it) {
        float frameTime = it->frameTime;
        sum += frameTime;
        sumSquares += frameTime * frameTime;
    }
    
    float mean = sum / count;
    float variance = (sumSquares / count) - (mean * mean);
    float stdDev = sqrt(variance);
    
    // Стабильным считается если стандартное отклонение < 20% от среднего
    return stdDev < (mean * 0.2f);
}

bool PerformanceMonitor::ShouldReduceQuality() const {
    return stats_.currentFPS < targetFPS_ * 0.8f || 
           stats_.currentFrameTime > (1000.0f / targetFPS_) * 1.3f;
}

bool PerformanceMonitor::ShouldIncreaseQuality() const {
    return IsFrameRateStable() && 
           stats_.currentFPS > targetFPS_ * 1.1f &&
           stats_.frameDropRate < 0.01f; // Менее 1% дропов
}

void PerformanceMonitor::ResetStats() {
    stats_ = PerformanceStats();
    frameCount_ = 0;
    fpsCounterStart_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::PrintReport() const {
    if (loggingEnabled_) {
        std::cout << "=== Performance Report ===\n";
        std::cout << "Current FPS: " << stats_.currentFPS << "\n";
        std::cout << "Average FPS: " << stats_.averageFPS << "\n";
        std::cout << "Frame Time: " << stats_.currentFrameTime << "ms\n";
        std::cout << "CPU Time: " << stats_.currentCpuTime << "ms\n";
        std::cout << "GPU Time: " << stats_.currentGpuTime << "ms\n";
        std::cout << "Memory Usage: " << stats_.usedMemory / (1024 * 1024) << "MB\n";
        std::cout << "Frame Drops: " << stats_.frameDrops << " (" << stats_.frameDropRate * 100 << "%)\n";
    }
}

void PerformanceMonitor::SaveReportToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "Performance Report\n";
        file << "Current FPS: " << stats_.currentFPS << "\n";
        file << "Average FPS: " << stats_.averageFPS << "\n";
        file << "Frame Time: " << stats_.currentFrameTime << "ms\n";
        file << "CPU Time: " << stats_.currentCpuTime << "ms\n";
        file << "GPU Time: " << stats_.currentGpuTime << "ms\n";
        file << "Memory Usage: " << stats_.usedMemory / (1024 * 1024) << "MB\n";
        file << "Frame Drops: " << stats_.frameDrops << "\n";
        file.close();
    }
}

void PerformanceMonitor::UpdateStats() {
    if (frameHistory_.empty()) return;
    
    // Обновляем текущие значения
    const auto& lastFrame = frameHistory_.back();
    stats_.currentFrameTime = lastFrame.frameTime;
    stats_.currentCpuTime = lastFrame.cpuTime;
    stats_.currentGpuTime = lastFrame.gpuTime;
    
    // Считаем FPS
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<float>(now - fpsCounterStart_).count();
    
    if (elapsed >= 1.0f) {
        stats_.currentFPS = frameCount_ / elapsed;
        if (OnFrameRateChanged) {
            OnFrameRateChanged(stats_.currentFPS);
        }
        frameCount_ = 0;
        fpsCounterStart_ = now;
    }
    
    // Считаем средние значения
    if (frameHistory_.size() > 1) {
        float totalFrameTime = 0.0f;
        float totalCpuTime = 0.0f;
        float totalGpuTime = 0.0f;
        
        for (const auto& frame : frameHistory_) {
            totalFrameTime += frame.frameTime;
            totalCpuTime += frame.cpuTime;
            totalGpuTime += frame.gpuTime;
        }
        
        size_t count = frameHistory_.size();
        stats_.averageFrameTime = totalFrameTime / count;
        stats_.averageCpuTime = totalCpuTime / count;
        stats_.averageGpuTime = totalGpuTime / count;
        stats_.averageFPS = 1000.0f / stats_.averageFrameTime;
        
        // Мин/макс значения
        auto minMaxFrame = std::minmax_element(frameHistory_.begin(), frameHistory_.end(),
            [](const FrameMetrics& a, const FrameMetrics& b) {
                return a.frameTime < b.frameTime;
            });
        
        stats_.minFrameTime = minMaxFrame.first->frameTime;
        stats_.maxFrameTime = minMaxFrame.second->frameTime;
        stats_.maxFPS = 1000.0f / stats_.minFrameTime;
        stats_.minFPS = 1000.0f / stats_.maxFrameTime;
    }
    
    stats_.totalFrames = frameCount_;
    if (stats_.totalFrames > 0) {
        stats_.frameDropRate = static_cast<float>(stats_.frameDrops) / stats_.totalFrames;
    }
}

void PerformanceMonitor::DetectPerformanceIssues() {
    if (frameHistory_.empty()) return;
    
    const auto& lastFrame = frameHistory_.back();
    
    // Детектируем дропы кадров
    if (lastFrame.frameTime > options_.hitchThreshold) {
        stats_.frameDrops++;
        
        if (OnPerformanceIssue) {
            OnPerformanceIssue();
        }
    }
}

// FramePacer реализация
FramePacer::FramePacer(float targetFPS) 
    : targetFPS_(targetFPS), frameInterval_(1.0f / targetFPS) {
    lastFrameTime_ = std::chrono::high_resolution_clock::now();
}

void FramePacer::WaitForNextFrame() {
    if (!vsyncEnabled_) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(now - lastFrameTime_).count();
        
        if (elapsed < frameInterval_) {
            float sleepTime = frameInterval_ - elapsed;
            std::this_thread::sleep_for(std::chrono::duration<float>(sleepTime));
        }
        
        lastFrameTime_ = std::chrono::high_resolution_clock::now();
    }
}

}} // namespace window_winapi::rendering
