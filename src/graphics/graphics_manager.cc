#include "graphics/graphics_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace WxeUI {

GraphicsManager::GraphicsManager()
    : currentAPI_(GraphicsAPI::DirectX12)
    , initialized_(false)
    , hwnd_(nullptr)
    , width_(0)
    , height_(0)
    , autoFallbackEnabled_(true)
    , performanceMonitoringEnabled_(false)
    , vsyncEnabled_(true)
    , frameRateLimit_(0)
    , highPerformanceMode_(true)
    , gpuSchedulingEnabled_(true) {
    
    // Стандартная цепочка fallback
    fallbackChain_ = {
        GraphicsAPI::DirectX12,
        GraphicsAPI::DirectX11,
        GraphicsAPI::Vulkan,
        GraphicsAPI::ANGLE,
        GraphicsAPI::Software
    };
}

GraphicsManager::~GraphicsManager() {
    Shutdown();
}

bool GraphicsManager::Initialize(HWND hwnd, int width, int height, GraphicsAPI preferredAPI) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    
    DetectSystemCapabilities();
    
    // Попробовать предпочитаемый API
    if (InitializeAPI(preferredAPI)) {
        currentAPI_ = preferredAPI;
        initialized_ = true;
        
        if (OnAPISwitch) {
            OnAPISwitch(currentAPI_, "Инициализация успешна");
        }
        
        return true;
    }
    
    // Если неудачно и включен auto fallback, попробовать другие API
    if (autoFallbackEnabled_) {
        return TryFallback();
    }
    
    return false;
}

bool GraphicsManager::InitializeAPI(GraphicsAPI api) {
    // Проверка доступности API
    switch (api) {
        case GraphicsAPI::DirectX12:
            if (!IsDirectX12Available()) return false;
            dx12Context_ = std::make_unique<DirectX12Context>();
            currentContext_ = std::unique_ptr<IGraphicsContext>(dx12Context_.get());
            break;
            
        case GraphicsAPI::DirectX11:
            if (!IsDirectX11Available()) return false;
            dx11Context_ = std::make_unique<DirectX11Context>();
            currentContext_ = std::unique_ptr<IGraphicsContext>(dx11Context_.get());
            break;
            
        case GraphicsAPI::Vulkan:
            if (!IsVulkanAvailable()) return false;
            vulkanContext_ = std::make_unique<VulkanContext>();
            currentContext_ = std::unique_ptr<IGraphicsContext>(vulkanContext_.get());
            break;
            
        case GraphicsAPI::ANGLE:
            if (!IsANGLEAvailable()) return false;
            angleContext_ = std::make_unique<ANGLEContext>();
            currentContext_ = std::unique_ptr<IGraphicsContext>(angleContext_.get());
            break;
            
        default:
            return false;
    }
    
    // Попытка инициализации контекста
    if (currentContext_->Initialize(hwnd_, width_, height_)) {
        if (performanceMonitoringEnabled_) {
            StartPerformanceMonitoring();
        }
        return true;
    }
    
    // Освобождение при неудаче
    currentContext_.reset();
    switch (api) {
        case GraphicsAPI::DirectX12: dx12Context_.reset(); break;
        case GraphicsAPI::DirectX11: dx11Context_.reset(); break;
        case GraphicsAPI::Vulkan: vulkanContext_.reset(); break;
        case GraphicsAPI::ANGLE: angleContext_.reset(); break;
    }
    
    failedAPIs_.push_back(api);
    return false;
}

bool GraphicsManager::TryFallback() {
    for (GraphicsAPI api : fallbackChain_) {
        // Пропустить уже неудачные API
        if (std::find(failedAPIs_.begin(), failedAPIs_.end(), api) != failedAPIs_.end()) {
            continue;
        }
        
        if (InitializeAPI(api)) {
            currentAPI_ = api;
            initialized_ = true;
            
            if (OnAPISwitch) {
                OnAPISwitch(currentAPI_, "Fallback к " + std::to_string(static_cast<int>(api)));
            }
            
            return true;
        }
    }
    
    if (OnError) {
        OnError("Все графические API недоступны");
    }
    
    return false;
}

bool GraphicsManager::SwitchAPI(GraphicsAPI newAPI) {
    if (!initialized_ || newAPI == currentAPI_) {
        return false;
    }
    
    // Сохранить старое состояние
    auto oldContext = std::move(currentContext_);
    GraphicsAPI oldAPI = currentAPI_;
    
    // Попытка инициализации нового API
    if (InitializeAPI(newAPI)) {
        // Успешно - освободить старый контекст
        if (oldContext) {
            oldContext->Shutdown();
        }
        
        currentAPI_ = newAPI;
        
        if (OnAPISwitch) {
            OnAPISwitch(newAPI, "Переключение с " + std::to_string(static_cast<int>(oldAPI)));
        }
        
        return true;
    } else {
        // Неудача - восстановить старый контекст
        currentContext_ = std::move(oldContext);
        
        if (OnError) {
            OnError("Не удалось переключиться на " + std::to_string(static_cast<int>(newAPI)));
        }
        
        return false;
    }
}

void GraphicsManager::Shutdown() {
    StopPerformanceMonitoring();
    
    if (currentContext_) {
        currentContext_->Shutdown();
        currentContext_.reset();
    }
    
    dx12Context_.reset();
    dx11Context_.reset();
    vulkanContext_.reset();
    angleContext_.reset();
    
    initialized_ = false;
}

void GraphicsManager::ResizeBuffers(int width, int height) {
    if (!initialized_ || !currentContext_) return;
    
    width_ = width;
    height_ = height;
    currentContext_->ResizeBuffers(width, height);
}

void GraphicsManager::Present() {
    if (!initialized_ || !currentContext_) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    currentContext_->Present();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000.0f;
    
    if (performanceMonitoringEnabled_) {
        frameTimes_.push_back(frameTime);
        if (frameTimes_.size() > 60) {
            frameTimes_.erase(frameTimes_.begin());
        }
        
        UpdatePerformanceMetrics();
    }
    
    lastFrameTime_ = endTime;
}

void GraphicsManager::Clear(float r, float g, float b, float a) {
    if (!initialized_ || !currentContext_) return;
    currentContext_->Clear(r, g, b, a);
}

sk_sp<SkSurface> GraphicsManager::GetSkiaSurface() {
    if (!initialized_ || !currentContext_) return nullptr;
    return currentContext_->GetSkiaSurface();
}

void GraphicsManager::WaitForGPU() {
    if (!initialized_ || !currentContext_) return;
    currentContext_->WaitForGPU();
}

std::vector<GraphicsManager::APICapabilities> GraphicsManager::EnumerateAPIs() const {
    std::vector<APICapabilities> capabilities;
    
    // DirectX 12
    APICapabilities dx12;
    dx12.api = GraphicsAPI::DirectX12;
    dx12.available = IsDirectX12Available();
    dx12.score = ScoreAPI(GraphicsAPI::DirectX12);
    dx12.supportsHDR = IsWindows10OrGreater();
    dx12.supportsRayTracing = dx12.available;
    dx12.supportsVariableRateShading = dx12.available;
    capabilities.push_back(dx12);
    
    // DirectX 11
    APICapabilities dx11;
    dx11.api = GraphicsAPI::DirectX11;
    dx11.available = IsDirectX11Available();
    dx11.score = ScoreAPI(GraphicsAPI::DirectX11);
    dx11.supportsHDR = false;
    dx11.supportsRayTracing = false;
    dx11.supportsVariableRateShading = false;
    capabilities.push_back(dx11);
    
    // Vulkan
    APICapabilities vulkan;
    vulkan.api = GraphicsAPI::Vulkan;
    vulkan.available = IsVulkanAvailable();
    vulkan.score = ScoreAPI(GraphicsAPI::Vulkan);
    vulkan.supportsHDR = vulkan.available;
    vulkan.supportsRayTracing = vulkan.available;
    vulkan.supportsVariableRateShading = vulkan.available;
    capabilities.push_back(vulkan);
    
    // ANGLE
    APICapabilities angle;
    angle.api = GraphicsAPI::ANGLE;
    angle.available = IsANGLEAvailable();
    angle.score = ScoreAPI(GraphicsAPI::ANGLE);
    angle.supportsHDR = false;
    angle.supportsRayTracing = false;
    angle.supportsVariableRateShading = false;
    capabilities.push_back(angle);
    
    return capabilities;
}

GraphicsManager::APICapabilities GraphicsManager::GetBestAPI() const {
    auto apis = EnumerateAPIs();
    
    auto best = std::max_element(apis.begin(), apis.end(),
        [](const APICapabilities& a, const APICapabilities& b) {
            if (!a.available) return true;
            if (!b.available) return false;
            return a.score < b.score;
        });
    
    return *best;
}

int GraphicsManager::ScoreAPI(GraphicsAPI api) const {
    int score = 0;
    
    switch (api) {
        case GraphicsAPI::DirectX12:
            if (IsWindows10OrGreater()) score += 100;
            if (HasDedicatedGPU()) score += 50;
            score += 90; // Базовый балл
            break;
            
        case GraphicsAPI::Vulkan:
            score += 85;
            if (HasDedicatedGPU()) score += 40;
            break;
            
        case GraphicsAPI::DirectX11:
            score += 70;
            if (HasDedicatedGPU()) score += 30;
            break;
            
        case GraphicsAPI::ANGLE:
            score += 50;
            break;
            
        default:
            score = 0;
            break;
    }
    
    return score;
}

void GraphicsManager::UpdatePerformanceMetrics() {
    if (frameTimes_.empty()) return;
    
    float avgFrameTime = 0;
    for (float time : frameTimes_) {
        avgFrameTime += time;
    }
    avgFrameTime /= frameTimes_.size();
    
    currentMetrics_.frameTime = avgFrameTime;
    currentMetrics_.fps = 1000.0f / avgFrameTime;
    
    if (OnPerformanceUpdate) {
        OnPerformanceUpdate(currentMetrics_);
    }
}

void GraphicsManager::StartPerformanceMonitoring() {
    performanceMonitoringEnabled_ = true;
    lastFrameTime_ = std::chrono::high_resolution_clock::now();
}

void GraphicsManager::StopPerformanceMonitoring() {
    performanceMonitoringEnabled_ = false;
    frameTimes_.clear();
}

GraphicsManager::PerformanceMetrics GraphicsManager::GetPerformanceMetrics() const {
    return currentMetrics_;
}

void GraphicsManager::DetectSystemCapabilities() {
    // Базовая детекция системы
    // В реальной реализации здесь был бы более сложный код
}

bool GraphicsManager::IsDirectX12Available() const {
    return IsWindows10OrGreater();
}

bool GraphicsManager::IsDirectX11Available() const {
    return true; // DirectX 11 доступен на всех поддерживаемых версиях Windows
}

bool GraphicsManager::IsVulkanAvailable() const {
    // Упрощенная проверка - в реальности нужно проверять драйвера
    return true;
}

bool GraphicsManager::IsANGLEAvailable() const {
    return true; // ANGLE обычно всегда доступен
}

bool GraphicsManager::IsWindows10OrGreater() const {
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    
    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask) != FALSE;
}

bool GraphicsManager::IsWindows11OrGreater() const {
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    
    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    osvi.dwBuildNumber = 22000; // Windows 11 build
    
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, dwlConditionMask) != FALSE;
}

bool GraphicsManager::HasDedicatedGPU() const {
    // Упрощенная проверка - в реальности нужно проверять через DXGI/WMI
    return true;
}

bool GraphicsManager::SupportsHDR() const {
    return IsWindows10OrGreater();
}

void GraphicsManager::SetVSync(bool enable) {
    vsyncEnabled_ = enable;
}

void GraphicsManager::SetFrameRateLimit(int fps) {
    frameRateLimit_ = fps;
}

void GraphicsManager::SetPowerMode(bool highPerformance) {
    highPerformanceMode_ = highPerformance;
}

void GraphicsManager::EnableGPUScheduling(bool enable) {
    gpuSchedulingEnabled_ = enable;
}

} // namespace window_winapi
