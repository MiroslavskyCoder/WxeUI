#include "features/openscreen.h"
#include "graphics/graphics_manager.h"
#include "memory/memory_manager.h"
#include "rendering/quality_manager.h"
#include "rendering/performance_monitor.h"
#include <algorithm>
#include <thread>

namespace WxeUI {
namespace features {

// OpenScreen Implementation
OpenScreen::OpenScreen() {
    // Инициализация системы совместного использования экранов
}

OpenScreen::~OpenScreen() {
    // Очистка всех экранов
    for (auto& [name, screenData] : screens_) {
        DestroyScreen(name);
    }
}

bool OpenScreen::CreateScreen(const std::string& screenName, const ScreenConfig& config) {
    if (screens_.find(screenName) != screens_.end()) {
        return false; // Экран уже существует
    }
    
    auto screenData = std::make_unique<ScreenData>();
    screenData->config = config;
    screenData->isActive = true;
    screenData->lastUpdate = std::chrono::steady_clock::now();
    
    // Создание графического контекста для совместного использования
    screenData->graphicsContext = graphics::GraphicsManager::CreateContext(
        config.enableHDR ? GraphicsAPI::DirectX12 : GraphicsAPI::DirectX11
    );
    
    if (!screenData->graphicsContext) {
        return false;
    }
    
    // Создание shared surface
    screenData->sharedSurface = SkSurface::MakeRenderTarget(
        screenData->graphicsContext->GetGrContext(),
        SkBudgeted::kYes,
        SkImageInfo::MakeN32Premul(config.width, config.height)
    );
    
    if (!screenData->sharedSurface) {
        return false;
    }
    
    screens_[screenName] = std::move(screenData);
    
    if (OnScreenCreated) {
        OnScreenCreated(screenName, config);
    }
    
    return true;
}

bool OpenScreen::DestroyScreen(const std::string& screenName) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return false;
    }
    
    // Отключение всех клиентов
    auto& screenData = it->second;
    for (const auto& client : screenData->clients) {
        if (OnClientDisconnected) {
            OnClientDisconnected(screenName, client.id);
        }
    }
    
    screens_.erase(it);
    
    if (OnScreenDestroyed) {
        OnScreenDestroyed(screenName);
    }
    
    return true;
}

bool OpenScreen::ShareScreen(const std::string& screenName, const std::string& clientId) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return false;
    }
    
    auto& screenData = it->second;
    
    // Проверка лимита клиентов
    if (screenData->clients.size() >= screenData->config.maxClients) {
        return false;
    }
    
    // Проверка разрешения на совместное использование
    if (!screenData->config.allowSharing) {
        return false;
    }
    
    // Добавление клиента
    ClientInfo client;
    client.id = clientId;
    client.displayName = "Client " + clientId;
    client.isActive = true;
    client.lastActivity = std::chrono::steady_clock::now();
    
    screenData->clients.push_back(client);
    
    if (OnClientConnected) {
        OnClientConnected(screenName, client);
    }
    
    return true;
}

bool OpenScreen::StopSharing(const std::string& screenName, const std::string& clientId) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return false;
    }
    
    auto& clients = it->second->clients;
    auto clientIt = std::find_if(clients.begin(), clients.end(),
        [&clientId](const ClientInfo& client) {
            return client.id == clientId;
        });
    
    if (clientIt == clients.end()) {
        return false;
    }
    
    clients.erase(clientIt);
    
    if (OnClientDisconnected) {
        OnClientDisconnected(screenName, clientId);
    }
    
    return true;
}

std::vector<std::string> OpenScreen::GetAvailableScreens() const {
    std::vector<std::string> screenNames;
    for (const auto& [name, screenData] : screens_) {
        if (screenData->config.allowSharing) {
            screenNames.push_back(name);
        }
    }
    return screenNames;
}

std::vector<OpenScreen::ClientInfo> OpenScreen::GetConnectedClients(const std::string& screenName) const {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return {};
    }
    return it->second->clients;
}

OpenScreen::ScreenConfig OpenScreen::GetScreenConfig(const std::string& screenName) const {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return {};
    }
    return it->second->config;
}

void OpenScreen::SetQualityLevel(const std::string& screenName, float quality) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return;
    }
    
    // Применение настроек качества через QualityManager
    rendering::QualityManager::SetGlobalQualityLevel(quality);
}

void OpenScreen::SetMaxFPS(const std::string& screenName, int maxFPS) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return;
    }
    
    // Применение ограничения FPS
    rendering::PerformanceMonitor::SetTargetFPS(maxFPS);
}

void OpenScreen::EnableAdaptiveQuality(const std::string& screenName, bool enable) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return;
    }
    
    // Включение адаптивного качества
    rendering::QualityManager::EnableAdaptiveQuality(enable);
}

void OpenScreen::UpdateScreen(const std::string& screenName) {
    auto it = screens_.find(screenName);
    if (it == screens_.end()) {
        return;
    }
    
    auto& screenData = it->second;
    screenData->lastUpdate = std::chrono::steady_clock::now();
    
    // Обновление shared surface для всех клиентов
    if (screenData->sharedSurface) {
        SkCanvas* canvas = screenData->sharedSurface->getCanvas();
        if (canvas) {
            // Здесь можно добавить рендеринг содержимого экрана
            canvas->clear(SK_ColorBLACK);
        }
    }
}

void OpenScreen::CleanupInactiveClients() {
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::minutes(5);
    
    for (auto& [screenName, screenData] : screens_) {
        auto& clients = screenData->clients;
        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                [now, timeout](const ClientInfo& client) {
                    return now - client.lastActivity > timeout;
                }),
            clients.end()
        );
    }
}

// FrameHigh Implementation
FrameHigh::FrameHigh(Window* window) : window_(window) {
    DetectDisplayCapabilities();
}

FrameHigh::~FrameHigh() {
    StopHighFrequencyRendering();
}

void FrameHigh::SetRenderConfig(const RenderConfig& config) {
    config_ = config;
    
    // Применение новых настроек
    if (isActive_) {
        StopHighFrequencyRendering();
        StartHighFrequencyRendering();
    }
}

void FrameHigh::StartHighFrequencyRendering() {
    if (isActive_) {
        return;
    }
    
    isActive_ = true;
    shouldStop_ = false;
    
    renderThread_ = std::make_unique<std::thread>(&FrameHigh::RenderLoop, this);
}

void FrameHigh::StopHighFrequencyRendering() {
    if (!isActive_) {
        return;
    }
    
    shouldStop_ = true;
    isActive_ = false;
    
    if (renderThread_ && renderThread_->joinable()) {
        renderThread_->join();
    }
    renderThread_.reset();
}

void FrameHigh::EnableAdaptiveRendering(bool enable) {
    config_.adaptiveRefreshRate = enable;
}

void FrameHigh::SetQualityThresholds(float minFPS, float targetFPS) {
    // Настройка порогов качества для адаптивного рендеринга
    rendering::QualityManager::SetPerformanceThresholds(minFPS, targetFPS);
}

void FrameHigh::RenderLoop() {
    const auto targetFrameTime = std::chrono::nanoseconds(1000000000 / config_.targetFPS);
    lastFrameTime_ = std::chrono::steady_clock::now();
    
    while (!shouldStop_) {
        auto frameStart = std::chrono::steady_clock::now();
        
        // Рендеринг кадра
        if (window_ && window_->IsValid()) {
            window_->Render();
        }
        
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameTime = frameEnd - frameStart;
        
        UpdateMetrics(std::chrono::duration<float, std::milli>(frameTime).count());
        
        // Адаптация качества если включена
        if (config_.adaptiveRefreshRate) {
            AdjustQuality();
        }
        
        // Ожидание до следующего кадра
        if (frameTime < targetFrameTime) {
            std::this_thread::sleep_for(targetFrameTime - frameTime);
        }
        
        if (OnPerformanceUpdate) {
            OnPerformanceUpdate(metrics_);
        }
    }
}

void FrameHigh::UpdateMetrics(float frameTime) {
    metrics_.frameTime = frameTime;
    metrics_.currentFPS = 1000.0f / frameTime;
    
    frameTimeHistory_.push_back(frameTime);
    if (frameTimeHistory_.size() > 60) {
        frameTimeHistory_.erase(frameTimeHistory_.begin());
    }
    
    // Вычисление среднего FPS
    if (!frameTimeHistory_.empty()) {
        float avgFrameTime = std::accumulate(frameTimeHistory_.begin(), frameTimeHistory_.end(), 0.0f) / frameTimeHistory_.size();
        metrics_.averageFPS = 1000.0f / avgFrameTime;
    }
    
    // Вычисление jitter
    if (frameTimeHistory_.size() > 1) {
        float variance = 0.0f;
        float mean = std::accumulate(frameTimeHistory_.begin(), frameTimeHistory_.end(), 0.0f) / frameTimeHistory_.size();
        
        for (float time : frameTimeHistory_) {
            variance += (time - mean) * (time - mean);
        }
        variance /= frameTimeHistory_.size();
        metrics_.jitter = std::sqrt(variance);
    }
}

void FrameHigh::AdjustQuality() {
    static float lastQualityAdjustment = 0.0f;
    
    if (metrics_.currentFPS < config_.targetFPS * 0.8f) {
        // Снижение качества
        float newQuality = std::max(0.1f, lastQualityAdjustment - 0.1f);
        rendering::QualityManager::SetGlobalQualityLevel(newQuality);
        lastQualityAdjustment = newQuality;
        
        if (OnQualityAdjustment) {
            OnQualityAdjustment(newQuality);
        }
    } else if (metrics_.currentFPS > config_.targetFPS * 1.1f) {
        // Повышение качества
        float newQuality = std::min(1.0f, lastQualityAdjustment + 0.05f);
        rendering::QualityManager::SetGlobalQualityLevel(newQuality);
        lastQualityAdjustment = newQuality;
        
        if (OnQualityAdjustment) {
            OnQualityAdjustment(newQuality);
        }
    }
}

void FrameHigh::DetectDisplayCapabilities() {
    // Определение возможностей дисплея
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(DEVMODE);
    
    if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode)) {
        config_.maxFPS = std::min(config_.maxFPS, static_cast<int>(devMode.dmDisplayFrequency));
    }
}

// MultiMonitorSupport Implementation
MultiMonitorSupport::MultiMonitorSupport() {
    RefreshMonitorList();
}

MultiMonitorSupport::~MultiMonitorSupport() {
}

std::vector<MultiMonitorSupport::MonitorInfo> MultiMonitorSupport::GetMonitors() const {
    return monitors_;
}

MultiMonitorSupport::MonitorInfo MultiMonitorSupport::GetPrimaryMonitor() const {
    for (const auto& monitor : monitors_) {
        if (monitor.isPrimary) {
            return monitor;
        }
    }
    return {};
}

MultiMonitorSupport::MonitorInfo MultiMonitorSupport::GetMonitorFromWindow(HWND hwnd) const {
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    
    for (const auto& monitor : monitors_) {
        if (monitor.handle == hMonitor) {
            return monitor;
        }
    }
    return {};
}

MultiMonitorSupport::MonitorInfo MultiMonitorSupport::GetMonitorFromPoint(POINT point) const {
    HMONITOR hMonitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    
    for (const auto& monitor : monitors_) {
        if (monitor.handle == hMonitor) {
            return monitor;
        }
    }
    return {};
}

bool MultiMonitorSupport::MoveWindowToMonitor(HWND hwnd, const MonitorInfo& monitor) {
    if (!hwnd) {
        return false;
    }
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    
    // Центрирование окна на мониторе
    int x = monitor.workArea.left + (monitor.workArea.right - monitor.workArea.left - windowWidth) / 2;
    int y = monitor.workArea.top + (monitor.workArea.bottom - monitor.workArea.top - windowHeight) / 2;
    
    return SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER) != FALSE;
}

bool MultiMonitorSupport::MaximizeWindowOnMonitor(HWND hwnd, const MonitorInfo& monitor) {
    if (!hwnd) {
        return false;
    }
    
    return SetWindowPos(hwnd, nullptr, 
        monitor.workArea.left, monitor.workArea.top,
        monitor.workArea.right - monitor.workArea.left,
        monitor.workArea.bottom - monitor.workArea.top,
        SWP_NOZORDER) != FALSE;
}

void MultiMonitorSupport::RefreshMonitorList() {
    monitors_.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(this));
}

BOOL CALLBACK MultiMonitorSupport::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto* multiMonitor = reinterpret_cast<MultiMonitorSupport*>(dwData);
    
    MonitorInfo info;
    info.handle = hMonitor;
    
    // Получение информации о мониторе
    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    
    if (GetMonitorInfo(hMonitor, &monitorInfo)) {
        info.bounds = monitorInfo.rcMonitor;
        info.workArea = monitorInfo.rcWork;
        info.isPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.name = monitorInfo.szDevice;
        
        // Получение DPI
        UINT dpiX, dpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            info.dpiX = static_cast<float>(dpiX);
            info.dpiY = static_cast<float>(dpiY);
        } else {
            info.dpiX = info.dpiY = 96.0f;
        }
        
        // Получение частоты обновления
        DEVMODE devMode = {};
        devMode.dmSize = sizeof(DEVMODE);
        if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
            info.refreshRate = devMode.dmDisplayFrequency;
        } else {
            info.refreshRate = 60;
        }
        
        // Определение поддержки HDR и Wide Color Gamut
        info.supportHDR = false; // Требует дополнительных проверок через DXGI
        info.supportWideColorGamut = false;
        
        multiMonitor->monitors_.push_back(info);
    }
    
    return TRUE;
}

} // namespace features
} // namespace window_winapi
