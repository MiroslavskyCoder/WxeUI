#include "rendering/quality_manager.h"
#include "window_winapi.h"
#include <iostream>
#include <algorithm>

namespace WxeUI {
namespace rendering {

QualityManager::QualityManager() {
    DetectHardwareCapabilities();
    settings_ = GetRecommendedSettings();
}

QualityManager::~QualityManager() {
}

void QualityManager::SetQualityLevel(QualityLevel level) {
    settings_.level = level;
    
    switch (level) {
        case QualityLevel::Low:
            settings_.antiAliasing = AntiAliasingType::None;
            settings_.enableHDR = false;
            settings_.enableWideColorGamut = false;
            settings_.enableShadows = false;
            settings_.enableBlur = false;
            settings_.maxTextureSize = 2048;
            break;
            
        case QualityLevel::Medium:
            settings_.antiAliasing = AntiAliasingType::MSAA_2X;
            settings_.enableHDR = false;
            settings_.enableWideColorGamut = false;
            settings_.enableShadows = true;
            settings_.enableBlur = true;
            settings_.maxTextureSize = 4096;
            break;
            
        case QualityLevel::High:
            settings_.antiAliasing = AntiAliasingType::MSAA_4X;
            settings_.enableHDR = true;
            settings_.enableWideColorGamut = true;
            settings_.enableShadows = true;
            settings_.enableBlur = true;
            settings_.maxTextureSize = 8192;
            break;
            
        case QualityLevel::Ultra:
            settings_.antiAliasing = AntiAliasingType::MSAA_8X;
            settings_.enableHDR = true;
            settings_.enableWideColorGamut = true;
            settings_.enableShadows = true;
            settings_.enableBlur = true;
            settings_.maxTextureSize = 16384;
            settings_.enableMipmaps = true;
            break;
    }
}

void QualityManager::SetQualitySettings(const QualitySettings& settings) {
    settings_ = settings;
}

void QualityManager::UpdatePerformanceInfo(const PerformanceInfo& info) {
    performanceInfo_ = info;
    
    if (adaptiveQuality_) {
        AdaptQualityToPerformance();
    }
}

void QualityManager::AdaptQualityToPerformance() {
    // Проверяем производительность
    bool shouldReduce = performanceInfo_.frameTime > maxFrameTime_ ||
                    performanceInfo_.cpuTime > maxCpuUsage_ ||
                    performanceInfo_.gpuTime > maxGpuUsage_ ||
                    performanceInfo_.isThrottling;
    
    bool shouldIncrease = performanceInfo_.frameTime < maxFrameTime_ * 0.7f &&
                    performanceInfo_.cpuTime < maxCpuUsage_ * 0.7f &&
                    performanceInfo_.gpuTime < maxGpuUsage_ * 0.7f &&
                    !performanceInfo_.isThrottling;
    
    if (shouldReduce) {
        OptimizeForPerformance();
    } else if (shouldIncrease) {
        OptimizeForQuality();
    }
}

void QualityManager::ApplyToSkiaContext(GrDirectContext* context) {
    if (!context) return;
    
    // Настраиваем качество рендеринга в Skia контексте
    GrContextOptions options;
    
    // Антиалиасинг
    switch (settings_.antiAliasing) {
        case AntiAliasingType::MSAA_2X:
            options.fInternalMultisampleCount = 2;
            break;
        case AntiAliasingType::MSAA_4X:
            options.fInternalMultisampleCount = 4;
            break;
        case AntiAliasingType::MSAA_8X:
            options.fInternalMultisampleCount = 8;
            break;
        default:
            options.fInternalMultisampleCount = 0;
            break;
    }
    
    // GPU кэширование
    if (settings_.enableGPUAcceleration) {
        context->setResourceCacheLimits(256, 256 * 1024 * 1024); // 256MB
    } else {
        context->setResourceCacheLimits(64, 64 * 1024 * 1024);   // 64MB
    }
}

SkImageInfo QualityManager::CreateOptimalImageInfo(int width, int height) {
    SkColorType colorType = settings_.enableWideColorGamut ? 
                        kRGBA_F16_SkColorType : kN32_SkColorType;
    
    SkColorSpace* colorSpace = settings_.enableWideColorGamut ?
                        SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, 
                                            SkNamedGamut::kDisplayP3).get() :
                        SkColorSpace::MakeSRGB().get();
    
    return SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType,
                        sk_ref_sp(colorSpace));
}

void QualityManager::DetectHardwareCapabilities() {
    // TODO: Реализовать детекцию возможностей GPU
    // Пока используем базовые настройки
}

QualitySettings QualityManager::GetRecommendedSettings() const {
    QualitySettings recommended;
    
    // Базовые рекомендации на основе детекции hardware
    recommended.level = QualityLevel::Medium;
    recommended.antiAliasing = AntiAliasingType::MSAA_4X;
    recommended.enableGPUAcceleration = true;
    recommended.enableTextureFiltering = true;
    
    return recommended;
}

void QualityManager::OptimizeForPerformance() {
    // Понижаем настройки для улучшения производительности
    if (settings_.antiAliasing == AntiAliasingType::MSAA_8X) {
        settings_.antiAliasing = AntiAliasingType::MSAA_4X;
    } else if (settings_.antiAliasing == AntiAliasingType::MSAA_4X) {
        settings_.antiAliasing = AntiAliasingType::MSAA_2X;
    } else if (settings_.antiAliasing == AntiAliasingType::MSAA_2X) {
        settings_.antiAliasing = AntiAliasingType::None;
    }
    
    if (settings_.maxTextureSize > 2048) {
        settings_.maxTextureSize /= 2;
    }
    
    settings_.enableBlur = false;
    settings_.enableShadows = false;
}

void QualityManager::OptimizeForQuality() {
    // Повышаем настройки при достаточной производительности
    if (settings_.antiAliasing == AntiAliasingType::None) {
        settings_.antiAliasing = AntiAliasingType::MSAA_2X;
    } else if (settings_.antiAliasing == AntiAliasingType::MSAA_2X) {
        settings_.antiAliasing = AntiAliasingType::MSAA_4X;
    } else if (settings_.antiAliasing == AntiAliasingType::MSAA_4X) {
        settings_.antiAliasing = AntiAliasingType::MSAA_8X;
    }
    
    if (settings_.maxTextureSize < 8192) {
        settings_.maxTextureSize *= 2;
    }
    
    settings_.enableBlur = true;
    settings_.enableShadows = true;
}

}} // namespace window_winapi::rendering
