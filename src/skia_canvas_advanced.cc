#include "window_winapi.h"
#include "memory/memory_manager.h"
#include "rendering/quality_manager.h"
#include "cache/fragment_cache.h"

namespace WxeUI {

// Расширенная реализация SkiaCanvas
SkiaCanvas::SkiaCanvas(sk_sp<SkSurface> surface) 
    : surface_(surface), cache_(nullptr) {
    if (surface_) {
        canvas_ = surface_->getCanvas();
    }
}

SkiaCanvas::~SkiaCanvas() {
    // Автоматическая очистка
}

void SkiaCanvas::Clear(SkColor color) {
    if (canvas_) {
        canvas_->clear(color);
    }
}

void SkiaCanvas::DrawRect(const SkRect& rect, const SkPaint& paint) {
    if (canvas_) {
        // Применение качества рендеринга
        SkPaint optimizedPaint = paint;
        float quality = rendering::QualityManager::GetGlobalQuality();
        
        if (quality < 0.5f) {
            optimizedPaint.setAntiAlias(false);
        } else if (quality < 0.8f) {
            optimizedPaint.setAntiAlias(true);
            optimizedPaint.setFilterQuality(kLow_SkFilterQuality);
        } else {
            optimizedPaint.setAntiAlias(true);
            optimizedPaint.setFilterQuality(kHigh_SkFilterQuality);
        }
        
        canvas_->drawRect(rect, optimizedPaint);
    }
}

void SkiaCanvas::DrawRoundRect(const SkRRect& rrect, const SkPaint& paint) {
    if (canvas_) {
        SkPaint optimizedPaint = paint;
        float quality = rendering::QualityManager::GetGlobalQuality();
        
        if (quality < 0.5f) {
            optimizedPaint.setAntiAlias(false);
        }
        
        canvas_->drawRRect(rrect, optimizedPaint);
    }
}

void SkiaCanvas::DrawText(const std::string& text, float x, float y, const SkFont& font, const SkPaint& paint) {
    if (canvas_) {
        SkPaint optimizedPaint = paint;
        SkFont optimizedFont = font;
        
        float quality = rendering::QualityManager::GetGlobalQuality();
        
        if (quality < 0.3f) {
            optimizedFont.setHinting(SkPaint::kNone_Hinting);
            optimizedPaint.setAntiAlias(false);
        } else if (quality < 0.7f) {
            optimizedFont.setHinting(SkPaint::kSlight_Hinting);
            optimizedPaint.setAntiAlias(true);
        } else {
            optimizedFont.setHinting(SkPaint::kFull_Hinting);
            optimizedPaint.setAntiAlias(true);
            optimizedFont.setSubpixel(true);
        }
        
        canvas_->drawString(text.c_str(), x, y, optimizedFont, optimizedPaint);
    }
}

void SkiaCanvas::DrawImage(sk_sp<SkImage> image, float x, float y, const SkPaint* paint) {
    if (canvas_ && image) {
        SkPaint optimizedPaint;
        if (paint) {
            optimizedPaint = *paint;
        }
        
        float quality = rendering::QualityManager::GetGlobalQuality();
        
        if (quality < 0.5f) {
            optimizedPaint.setFilterQuality(kNone_SkFilterQuality);
        } else if (quality < 0.8f) {
            optimizedPaint.setFilterQuality(kLow_SkFilterQuality);
        } else {
            optimizedPaint.setFilterQuality(kHigh_SkFilterQuality);
        }
        
        canvas_->drawImage(image, x, y, &optimizedPaint);
    }
}

void SkiaCanvas::BeginFragment(const std::string& fragmentId) {
    currentFragment_ = fragmentId;
    
    if (cache_ && cache_->IsFragmentCached(fragmentId)) {
        // Использование кэшированного фрагмента
        auto cachedSurface = cache_->GetCachedSurface(fragmentId, surface_->width(), surface_->height());
        if (cachedSurface) {
            canvas_->drawImage(cachedSurface->makeImageSnapshot(), 0, 0);
        }
    }
}

void SkiaCanvas::EndFragment() {
    if (cache_ && !currentFragment_.empty()) {
        // Кэширование текущего фрагмента
        auto fragmentSurface = ToFrame(surface_->width(), surface_->height());
        if (fragmentSurface) {
            cache_->CacheSurface(currentFragment_, fragmentSurface);
        }
    }
    
    currentFragment_.clear();
}

bool SkiaCanvas::IsFragmentCached(const std::string& fragmentId) const {
    return cache_ && cache_->IsFragmentCached(fragmentId);
}

sk_sp<SkSurface> SkiaCanvas::ToFrame(int width, int height) const {
    if (!surface_) {
        return nullptr;
    }
    
    auto info = SkImageInfo::MakeN32Premul(width, height);
    auto newSurface = SkSurface::MakeRaster(info);
    
    if (newSurface && surface_) {
        auto image = surface_->makeImageSnapshot();
        if (image) {
            newSurface->getCanvas()->drawImage(image, 0, 0);
        }
    }
    
    return newSurface;
}

} // namespace window_winapi
