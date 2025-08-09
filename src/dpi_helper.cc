#include "window_winapi.h"
#include <shellscalingapi.h>
#include <versionhelpers.h>

#pragma comment(lib, "shcore.lib")

namespace WxeUI {

// Установка DPI Awareness
bool DPIHelper::SetDPIAwareness(DPIAwareness awareness) {
    if (!IsWindows8Point1OrGreater()) {
        return false;
    }
    
    switch (awareness) {
        case DPIAwareness::Unaware:
            return SUCCEEDED(SetProcessDpiAwareness(PROCESS_DPI_UNAWARE));
        
        case DPIAwareness::System:
            return SUCCEEDED(SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE));
        
        case DPIAwareness::PerMonitor:
            return SUCCEEDED(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE));
        
        case DPIAwareness::PerMonitorV2:
            if (IsWindows10OrGreater()) {
                typedef BOOL(WINAPI* SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
                HMODULE user32 = GetModuleHandleW(L"user32.dll");
                if (user32) {
                    auto setProcessDpiAwarenessContext = 
                        reinterpret_cast<SetProcessDpiAwarenessContextProc>(
                            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
                    if (setProcessDpiAwarenessContext) {
                        return setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                    }
                }
            }
            // Fallback to PerMonitor
            return SUCCEEDED(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE));
        
        default:
            return false;
    }
}

// Получение масштаба DPI
float DPIHelper::GetDPIScale(HWND hwnd) {
    if (IsWindows8Point1OrGreater()) {
        typedef UINT(WINAPI* GetDpiForWindowProc)(HWND);
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            auto getDpiForWindow = reinterpret_cast<GetDpiForWindowProc>(
                GetProcAddress(user32, "GetDpiForWindow"));
            if (getDpiForWindow) {
                UINT dpi = getDpiForWindow(hwnd);
                return static_cast<float>(dpi) / 96.0f;
            }
        }
        
        // Fallback для Windows 8.1
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX, dpiY;
        if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            return static_cast<float>(dpiX) / 96.0f;
        }
    }
    
    // Fallback для старых версий Windows
    HDC dc = GetDC(hwnd);
    if (dc) {
        int dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(hwnd, dc);
        return static_cast<float>(dpi) / 96.0f;
    }
    
    return 1.0f;
}

// Получение DPI по осям
void DPIHelper::GetDPI(HWND hwnd, float* dpiX, float* dpiY) {
    if (!dpiX || !dpiY) {
        return;
    }
    
    if (IsWindows8Point1OrGreater()) {
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT x, y;
        if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y))) {
            *dpiX = static_cast<float>(x);
            *dpiY = static_cast<float>(y);
            return;
        }
    }
    
    // Fallback
    HDC dc = GetDC(hwnd);
    if (dc) {
        *dpiX = static_cast<float>(GetDeviceCaps(dc, LOGPIXELSX));
        *dpiY = static_cast<float>(GetDeviceCaps(dc, LOGPIXELSY));
        ReleaseDC(hwnd, dc);
    } else {
        *dpiX = *dpiY = 96.0f;
    }
}

// Масштабирование прямоугольника
RECT DPIHelper::ScaleRect(const RECT& rect, float scale) {
    RECT scaledRect;
    scaledRect.left = static_cast<LONG>(rect.left * scale);
    scaledRect.top = static_cast<LONG>(rect.top * scale);
    scaledRect.right = static_cast<LONG>(rect.right * scale);
    scaledRect.bottom = static_cast<LONG>(rect.bottom * scale);
    return scaledRect;
}

// Получение информации о дисплее
DisplayInfo DPIHelper::GetDisplayInfo(HWND hwnd) {
    DisplayInfo info = {};
    
    info.monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(info.monitor, &mi)) {
        info.workArea = mi.rcWork;
        info.monitorArea = mi.rcMonitor;
    }
    
    GetDPI(hwnd, &info.dpiX, &info.dpiY);
    info.scaleFactor = GetDPIScale(hwnd);
    
    return info;
}

// Корректировка размера окна для DPI
void DPIHelper::AdjustWindowRectForDPI(RECT* rect, DWORD style, DWORD exStyle, HWND hwnd) {
    if (!rect) {
        return;
    }
    
    if (IsWindows10OrGreater()) {
        typedef BOOL(WINAPI* AdjustWindowRectExForDpiProc)(LPRECT, DWORD, BOOL, DWORD, UINT);
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            auto adjustWindowRectExForDpi = 
                reinterpret_cast<AdjustWindowRectExForDpiProc>(
                    GetProcAddress(user32, "AdjustWindowRectExForDpi"));
            if (adjustWindowRectExForDpi) {
                UINT dpi = hwnd ? static_cast<UINT>(GetDPIScale(hwnd) * 96.0f) : 96;
                adjustWindowRectExForDpi(rect, style, FALSE, exStyle, dpi);
                return;
            }
        }
    }
    
    // Fallback
    AdjustWindowRectEx(rect, style, FALSE, exStyle);
}

} // namespace window_winapi
