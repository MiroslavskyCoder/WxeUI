#pragma once

#include <windows.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include "window_winapi.h"

namespace WxeUI {
namespace features {

// Система совместного использования экранов
class OpenScreen {
public:
    struct ScreenConfig {
        std::string name;
        int width = 1920;
        int height = 1080;
        bool allowSharing = true;
        bool enableMirroring = false;
        int maxClients = 4;
        bool enableHDR = false;
        bool enableWideColorGamut = false;
    };
    
    struct ClientInfo {
        std::string id;
        std::string displayName;
        HWND windowHandle;
        bool isActive = true;
        std::chrono::steady_clock::time_point lastActivity;
    };
    
    OpenScreen();
    ~OpenScreen();
    
    // Создание и управление экранами
    bool CreateScreen(const std::string& screenName, const ScreenConfig& config);
    bool DestroyScreen(const std::string& screenName);
    bool ShareScreen(const std::string& screenName, const std::string& clientId);
    bool StopSharing(const std::string& screenName, const std::string& clientId);
    
    // Получение информации
    std::vector<std::string> GetAvailableScreens() const;
    std::vector<ClientInfo> GetConnectedClients(const std::string& screenName) const;
    ScreenConfig GetScreenConfig(const std::string& screenName) const;
    
    // Управление качеством и производительностью
    void SetQualityLevel(const std::string& screenName, float quality);
    void SetMaxFPS(const std::string& screenName, int maxFPS);
    void EnableAdaptiveQuality(const std::string& screenName, bool enable);
    
    // События
    std::function<void(const std::string&, const ClientInfo&)> OnClientConnected;
    std::function<void(const std::string&, const std::string&)> OnClientDisconnected;
    std::function<void(const std::string&, const ScreenConfig&)> OnScreenCreated;
    std::function<void(const std::string&)> OnScreenDestroyed;
    
private:
    struct ScreenData {
        ScreenConfig config;
        std::vector<ClientInfo> clients;
        sk_sp<SkSurface> sharedSurface;
        std::unique_ptr<IGraphicsContext> graphicsContext;
        bool isActive = false;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    std::unordered_map<std::string, std::unique_ptr<ScreenData>> screens_;
    
    void UpdateScreen(const std::string& screenName);
    void CleanupInactiveClients();
};

// Высокочастотный рендеринг
class FrameHigh {
public:
    struct RenderConfig {
        int targetFPS = 120;
        int maxFPS = 240;
        bool enableVSync = false;
        bool enableFreeSync = true;
        bool enableGSync = true;
        bool adaptiveRefreshRate = true;
        bool enableTearing = false;
    };
    
    struct PerformanceMetrics {
        float currentFPS = 0.0f;
        float averageFPS = 0.0f;
        float frameTime = 0.0f;
        float cpuTime = 0.0f;
        float gpuTime = 0.0f;
        int droppedFrames = 0;
        float jitter = 0.0f;
    };
    
    FrameHigh(Window* window);
    ~FrameHigh();
    
    // Настройка рендеринга
    void SetRenderConfig(const RenderConfig& config);
    RenderConfig GetRenderConfig() const { return config_; }
    
    // Управление рендерингом
    void StartHighFrequencyRendering();
    void StopHighFrequencyRendering();
    bool IsRenderingActive() const { return isActive_; }
    
    // Адаптивная настройка
    void EnableAdaptiveRendering(bool enable);
    void SetQualityThresholds(float minFPS, float targetFPS);
    
    // Получение метрик
    PerformanceMetrics GetPerformanceMetrics() const { return metrics_; }
    
    // События
    std::function<void(const PerformanceMetrics&)> OnPerformanceUpdate;
    std::function<void(float)> OnQualityAdjustment;
    
private:
    Window* window_;
    RenderConfig config_;
    PerformanceMetrics metrics_;
    bool isActive_ = false;
    
    std::unique_ptr<std::thread> renderThread_;
    std::atomic<bool> shouldStop_{false};
    
    std::vector<float> frameTimeHistory_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    
    void RenderLoop();
    void UpdateMetrics(float frameTime);
    void AdjustQuality();
    void DetectDisplayCapabilities();
};

// Мульти-монитор поддержка
class MultiMonitorSupport {
public:
    struct MonitorInfo {
        HMONITOR handle;
        std::string name;
        RECT bounds;
        RECT workArea;
        float dpiX, dpiY;
        int refreshRate;
        bool supportHDR;
        bool supportWideColorGamut;
        bool isPrimary;
    };
    
    MultiMonitorSupport();
    ~MultiMonitorSupport();
    
    // Получение информации о мониторах
    std::vector<MonitorInfo> GetMonitors() const;
    MonitorInfo GetPrimaryMonitor() const;
    MonitorInfo GetMonitorFromWindow(HWND hwnd) const;
    MonitorInfo GetMonitorFromPoint(POINT point) const;
    
    // Управление окнами на мониторах
    bool MoveWindowToMonitor(HWND hwnd, const MonitorInfo& monitor);
    bool MaximizeWindowOnMonitor(HWND hwnd, const MonitorInfo& monitor);
    
    // События изменения конфигурации мониторов
    std::function<void(const std::vector<MonitorInfo>&)> OnMonitorConfigChanged;
    
private:
    std::vector<MonitorInfo> monitors_;
    
    void RefreshMonitorList();
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
};

} // namespace features
} // namespace window_winapi
