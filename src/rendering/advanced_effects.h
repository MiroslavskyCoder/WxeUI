#pragma once

#include "window_winapi.h"
#include "include/effects/SkImageFilters.h"
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkBlurImageFilter.h"
#include "include/effects/SkDropShadowImageFilter.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPathEffect.h"

namespace WxeUI {
namespace rendering {

// Настройки размытия
struct BlurSettings {
    float sigmaX = 5.0f;
    float sigmaY = 5.0f;
    SkTileMode tileMode = SkTileMode::kClamp;
    bool highQuality = true;
};

// Настройки тени
struct ShadowSettings {
    float offsetX = 3.0f;
    float offsetY = 3.0f;
    float blurRadius = 5.0f;
    SkColor color = SkColorSetARGB(128, 0, 0, 0);
    bool innerShadow = false;
};

// Настройки градиента
struct GradientSettings {
    std::vector<SkColor> colors;
    std::vector<float> positions;
    SkTileMode tileMode = SkTileMode::kClamp;
    SkMatrix localMatrix = SkMatrix::I();
};

// Настройки маски
struct MaskSettings {
    sk_sp<SkImage> maskImage;
    SkBlendMode blendMode = SkBlendMode::kSrcIn;
    bool invertMask = false;
    float opacity = 1.0f;
};

class AdvancedEffects {
public:
    AdvancedEffects();
    ~AdvancedEffects();
    
    // Размытие и фильтры
    sk_sp<SkImageFilter> CreateBlurFilter(const BlurSettings& settings);
    sk_sp<SkImageFilter> CreateGaussianBlur(float sigma);
    sk_sp<SkImageFilter> CreateMotionBlur(float angle, float distance);
    sk_sp<SkImageFilter> CreateRadialBlur(const SkPoint& center, float angle);
    
    // Тени и свечение
    sk_sp<SkImageFilter> CreateDropShadow(const ShadowSettings& settings);
    sk_sp<SkImageFilter> CreateInnerShadow(const ShadowSettings& settings);
    sk_sp<SkImageFilter> CreateGlow(SkColor color, float radius, float intensity);
    sk_sp<SkImageFilter> CreateBevel(float depth, float angle, SkColor highlightColor, SkColor shadowColor);
    
    // Градиенты
    sk_sp<SkShader> CreateLinearGradient(const SkPoint& start, const SkPoint& end, const GradientSettings& settings);
    sk_sp<SkShader> CreateRadialGradient(const SkPoint& center, float radius, const GradientSettings& settings);
    sk_sp<SkShader> CreateConicGradient(const SkPoint& center, float startAngle, const GradientSettings& settings);
    sk_sp<SkShader> CreateSweepGradient(const SkPoint& center, const GradientSettings& settings);
    
    // Маски и clipping
    void ApplyMask(SkCanvas* canvas, const MaskSettings& settings, const SkRect& bounds);
    void BeginClipPath(SkCanvas* canvas, const SkPath& path, bool antiAlias = true);
    void EndClipPath(SkCanvas* canvas);
    
    // Цветовые эффекты
    sk_sp<SkImageFilter> CreateColorMatrix(const float colorMatrix[20]);
    sk_sp<SkImageFilter> CreateHueRotation(float degrees);
    sk_sp<SkImageFilter> CreateSaturation(float saturation);
    sk_sp<SkImageFilter> CreateBrightness(float brightness);
    sk_sp<SkImageFilter> CreateContrast(float contrast);
    sk_sp<SkImageFilter> CreateSepia();
    sk_sp<SkImageFilter> CreateGrayscale();
    
    // Дисторсия и искажения
    sk_sp<SkImageFilter> CreateDisplacement(sk_sp<SkImage> displacementMap, float scale);
    sk_sp<SkImageFilter> CreateMorphology(SkImageFilters::Morphology type, float radiusX, float radiusY);
    sk_sp<SkImageFilter> CreateTurbulence(float baseFreqX, float baseFreqY, int numOctaves);
    
    // Композиция эффектов
    sk_sp<SkImageFilter> ComposeFilters(sk_sp<SkImageFilter> outer, sk_sp<SkImageFilter> inner);
    sk_sp<SkImageFilter> BlendFilters(sk_sp<SkImageFilter> background, sk_sp<SkImageFilter> foreground, SkBlendMode mode);
    
    // Готовые комбинации эффектов
    sk_sp<SkImageFilter> CreateGlowingText(SkColor glowColor, float radius);
    sk_sp<SkImageFilter> CreateEmbossedLook(float depth, float angle);
    sk_sp<SkImageFilter> CreateGlassEffect(float refraction);
    sk_sp<SkImageFilter> CreateVintagePhoto();
    
    // Продвинутые шейдеры
    sk_sp<SkShader> CreateNoiseShader(float scale, bool turbulence = false);
    sk_sp<SkShader> CreatePerlinNoise(float baseFreqX, float baseFreqY, int numOctaves);
    sk_sp<SkShader> CreateTextureShader(sk_sp<SkImage> texture, SkTileMode tmx, SkTileMode tmy);
    
private:
    // Кэширование часто используемых фильтров
    std::unordered_map<std::string, sk_sp<SkImageFilter>> filterCache_;
    std::unordered_map<std::string, sk_sp<SkShader>> shaderCache_;
    
    std::string HashSettings(const void* settings, size_t size);
    void ClearOldCacheEntries();
};

}} // namespace window_winapi::rendering
