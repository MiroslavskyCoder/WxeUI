#pragma once

#include "window_winapi.h"
#include "include/gpu/GrDirectContext.h"
#include "include/core/SkImageInfo.h"

namespace WxeUI {
namespace rendering {

// Уровни качества рендеринга
enum class QualityLevel {
    Low,     // Базовая производительность, минимальные эффекты
    Medium,  // Сбалансированные настройки
    High,    // Высокое качество с расширенными эффектами
    Ultra    // Максимальное качество, все возможные улучшения
};

// Типы антиалиасинга
enum class AntiAliasingType {
    None,
    MSAA_2X,
    MSAA_4X,
    MSAA_8X,
    FXAA,
    TAA
};

// Настройки качества
struct QualitySettings {
    QualityLevel level = QualityLevel::Medium;
    AntiAliasingType antiAliasing = AntiAliasingType::MSAA_4X;
    bool enableHDR = false;
    bool enableWideColorGamut = false;
    bool enableGPUAcceleration = true;
    bool enableTextureFiltering = true;
    bool enableShadows = true;
    bool enableBlur = true;
    int maxTextureSize = 8192;
    float lodBias = 0.0f;
    bool enableMipmaps = true;
};

// Информация о производительности
struct PerformanceInfo {
    float frameTime = 0.0f;
    float cpuTime = 0.0f;
    float gpuTime = 0.0f;
    float memoryUsage = 0.0f;
    float temperature = 0.0f;
    bool isThrottling = false;
};

class QualityManager {
public:
    QualityManager();
    ~QualityManager();
    
    // Управление качеством
    void SetQualityLevel(QualityLevel level);
    QualityLevel GetQualityLevel() const { return settings_.level; }
    
    void SetQualitySettings(const QualitySettings& settings);
    const QualitySettings& GetQualitySettings() const { return settings_; }
    
    // Адаптивное управление качеством
    void EnableAdaptiveQuality(bool enable) { adaptiveQuality_ = enable; }
    bool IsAdaptiveQualityEnabled() const { return adaptiveQuality_; }
    
    void UpdatePerformanceInfo(const PerformanceInfo& info);
    void AdaptQualityToPerformance();
    
    // Применение настроек к Skia контексту
    void ApplyToSkiaContext(GrDirectContext* context);
    SkImageInfo CreateOptimalImageInfo(int width, int height);
    
    // GPU детектирование и рекомендации
    void DetectHardwareCapabilities();
    QualitySettings GetRecommendedSettings() const;
    
private:
    QualitySettings settings_;
    PerformanceInfo performanceInfo_;
    bool adaptiveQuality_ = false;
    
    // Целевые значения производительности
    float targetFrameRate_ = 60.0f;
    float maxFrameTime_ = 16.67f; // 60 FPS
    float maxCpuUsage_ = 80.0f;
    float maxGpuUsage_ = 85.0f;
    
    void OptimizeForPerformance();
    void OptimizeForQuality();
};

}} // namespace window_winapi::rendering
