#include "window_winapi.h"
#include <iostream>

namespace WxeUI {

// ================== FragmentCache ==================

sk_sp<SkSurface> FragmentCache::GetCachedSurface(const std::string& key, int width, int height) {
    auto it = cache_.find(key);
    
    if (it != cache_.end()) {
        // Обновляем время последнего использования
        it->second.lastUsed = std::chrono::steady_clock::now();
        
        // Проверяем, подходит ли размер
        auto surface = it->second.surface;
        if (surface && surface->width() == width && surface->height() == height) {
            return surface;
        }
        
        // Если размер не подходит, удаляем запись
        cache_.erase(it);
    }
    
    // Создаем новую поверхность
    SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
    
    if (surface) {
        // Добавляем в кэш
        CacheEntry entry;
        entry.surface = surface;
        entry.lastUsed = std::chrono::steady_clock::now();
        entry.hash = std::hash<std::string>{}(key + std::to_string(width) + std::to_string(height));
        entry.isDirty = false;
        
        cache_[key] = entry;
        
        // Проверяем размер кэша
        if (cache_.size() > maxCacheSize_) {
            GarbageCollect();
        }
    }
    
    return surface;
}

void FragmentCache::InvalidateCache(const std::string& key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        it->second.isDirty = true;
    }
}

void FragmentCache::ClearCache() {
    cache_.clear();
}

void FragmentCache::SetMaxCacheSize(size_t maxSize) {
    maxCacheSize_ = maxSize;
    if (cache_.size() > maxCacheSize_) {
        GarbageCollect();
    }
}

void FragmentCache::GarbageCollect() {
    // Удаляем старые записи
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = cache_.begin(); it != cache_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.lastUsed);
        
        if (age > maxAge_ || it->second.isDirty) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Если кэш все еще слишком большой, удаляем самые старые записи
    while (cache_.size() > maxCacheSize_) {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.lastUsed < oldest->second.lastUsed) {
                oldest = it;
            }
        }
        cache_.erase(oldest);
    }
}

// ================== SkiaCanvas ==================

SkiaCanvas::SkiaCanvas(sk_sp<SkSurface> surface)
    : surface_(surface)
    , canvas_(surface ? surface->getCanvas() : nullptr)
    , cache_(nullptr) {
}

SkiaCanvas::~SkiaCanvas() {
    // canvas_ принадлежит surface_, не нужно удалять
}

void SkiaCanvas::Clear(SkColor color) {
    if (canvas_) {
        canvas_->clear(color);
    }
}

void SkiaCanvas::DrawRect(const SkRect& rect, const SkPaint& paint) {
    if (canvas_) {
        canvas_->drawRect(rect, paint);
    }
}

void SkiaCanvas::DrawRoundRect(const SkRRect& rrect, const SkPaint& paint) {
    if (canvas_) {
        canvas_->drawRRect(rrect, paint);
    }
}

void SkiaCanvas::DrawText(const std::string& text, float x, float y, const SkFont& font, const SkPaint& paint) {
    if (canvas_) {
        canvas_->drawString(text.c_str(), x, y, font, paint);
    }
}

void SkiaCanvas::DrawImage(sk_sp<SkImage> image, float x, float y, const SkPaint* paint) {
    if (canvas_ && image) {
        canvas_->drawImage(image, x, y, paint);
    }
}

void SkiaCanvas::BeginFragment(const std::string& fragmentId) {
    currentFragment_ = fragmentId;
    // TODO: Реализация фрагментированного рендеринга
    // Можно сохранить состояние canvas_ для последующего кэширования
}

void SkiaCanvas::EndFragment() {
    // TODO: Завершение фрагмента и сохранение в кэш
    currentFragment_.clear();
}

bool SkiaCanvas::IsFragmentCached(const std::string& fragmentId) const {
    // TODO: Проверка наличия фрагмента в кэше
    return false;
}

// ================== Utility Functions ==================

namespace utils {
    std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
        
        return result;
    }
    
    std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return {};
        
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
        
        return result;
    }
    
    bool IsWindows10OrGreater() {
        OSVERSIONINFOEXW osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        osvi.dwMajorVersion = 10;
        osvi.dwMinorVersion = 0;
        
        DWORDLONG conditionMask = 0;
        VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
        
        return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) != FALSE;
    }
    
    bool IsWindows11OrGreater() {
        OSVERSIONINFOEXW osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        osvi.dwMajorVersion = 10;
        osvi.dwMinorVersion = 0;
        osvi.dwBuildNumber = 22000; // Windows 11 build
        
        DWORDLONG conditionMask = 0;
        VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(conditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
        
        return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, conditionMask) != FALSE;
    }
    
    RECT GetMonitorWorkArea(HMONITOR monitor) {
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) {
            return mi.rcWork;
        }
        return {};
    }
    
    std::vector<DisplayInfo> EnumerateDisplays() {
        std::vector<DisplayInfo> displays;
        
        // Callback для перечисления мониторов
        auto enumProc = [](HMONITOR monitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
            auto* displays = reinterpret_cast<std::vector<DisplayInfo>*>(lParam);
            
            DisplayInfo info = {};
            info.monitor = monitor;
            
            MONITORINFO mi = {};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(monitor, &mi)) {
                info.workArea = mi.rcWork;
                info.monitorArea = mi.rcMonitor;
            }
            
            // Получение DPI для монитора
            UINT dpiX, dpiY;
            if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                info.dpiX = static_cast<float>(dpiX);
                info.dpiY = static_cast<float>(dpiY);
                info.scaleFactor = static_cast<float>(dpiX) / 96.0f;
            } else {
                info.dpiX = info.dpiY = 96.0f;
                info.scaleFactor = 1.0f;
            }
            
            displays->push_back(info);
            return TRUE;
        };
        
        EnumDisplayMonitors(nullptr, nullptr, enumProc, reinterpret_cast<LPARAM>(&displays));
        return displays;
    }
}

} // namespace window_winapi
