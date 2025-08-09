#pragma once

#include "window_winapi.h"
#include "directx12_context.h"
#include "directx11_context.h"
#include "vulkan_context.h"
#include "angle_context.h"
#include <memory>
#include <vector>
#include <chrono>
#include <functional>

namespace WxeUI {

class GraphicsManager {
public:
    GraphicsManager();
    ~GraphicsManager();
    
    // Инициализация с автоматическим выбором лучшего API
    bool Initialize(HWND hwnd, int width, int height, GraphicsAPI preferredAPI = GraphicsAPI::DirectX12);
    void Shutdown();
    
    // Переключение между API в runtime
    bool SwitchAPI(GraphicsAPI newAPI);
    
    // Унифицированный интерфейс
    void ResizeBuffers(int width, int height);
    void Present();
    void Clear(float r, float g, float b, float a);
    sk_sp<SkSurface> GetSkiaSurface();
    void WaitForGPU();
    
    // Информация о текущем состоянии
    GraphicsAPI GetCurrentAPI() const { return currentAPI_; }
    bool IsInitialized() const { return initialized_; }
    
    // Определение лучшего API
    struct APICapabilities {
        GraphicsAPI api;
        bool available;
        int score;
        std::string deviceName;
        std::string driverVersion;
        size_t dedicatedMemory;
        bool supportsHDR;
        bool supportsRayTracing;
        bool supportsVariableRateShading;
        float performanceScore;
    };
    
    std::vector<APICapabilities> EnumerateAPIs() const;
    APICapabilities GetBestAPI() const;
    
    // Мониторинг производительности
    struct PerformanceMetrics {
        float frameTime;        // мс
        float fps;
        float cpuUsage;         // %
        float gpuUsage;         // %
        size_t memoryUsage;     // байт
        float temperature;      // °C
        int droppedFrames;
    };
    
    PerformanceMetrics GetPerformanceMetrics() const;
    void StartPerformanceMonitoring();
    void StopPerformanceMonitoring();
    
    // Fallback механизм
    void EnableAutoFallback(bool enable) { autoFallbackEnabled_ = enable; }
    bool IsAutoFallbackEnabled() const { return autoFallbackEnabled_; }
    void SetFallbackChain(const std::vector<GraphicsAPI>& chain) { fallbackChain_ = chain; }
    
    // Оптимизации
    void SetVSync(bool enable);
    void SetFrameRateLimit(int fps);
    void SetPowerMode(bool highPerformance);
    void EnableGPUScheduling(bool enable);
    
    // Коллбэки для событий
    std::function<void(GraphicsAPI, const std::string&)> OnAPISwitch;
    std::function<void(const PerformanceMetrics&)> OnPerformanceUpdate;
    std::function<void(const std::string&)> OnError;
    
private:
    // Активный контекст
    std::unique_ptr<IGraphicsContext> currentContext_;
    GraphicsAPI currentAPI_;
    bool initialized_;
    
    // Контексты для всех API
    std::unique_ptr<DirectX12Context> dx12Context_;
    std::unique_ptr<DirectX11Context> dx11Context_;
    std::unique_ptr<VulkanContext> vulkanContext_;
    std::unique_ptr<ANGLEContext> angleContext_;
    
    // Параметры
    HWND hwnd_;
    int width_, height_;
    
    // Fallback
    bool autoFallbackEnabled_;
    std::vector<GraphicsAPI> fallbackChain_;
    std::vector<GraphicsAPI> failedAPIs_;
    
    // Мониторинг
    bool performanceMonitoringEnabled_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    PerformanceMetrics currentMetrics_;
    std::vector<float> frameTimes_;
    
    // Оптимизации
    bool vsyncEnabled_;
    int frameRateLimit_;
    bool highPerformanceMode_;
    bool gpuSchedulingEnabled_;
    
    // Помощники методы
    bool InitializeAPI(GraphicsAPI api);
    int ScoreAPI(GraphicsAPI api) const;
    bool TryFallback();
    void UpdatePerformanceMetrics();
    APICapabilities GetAPICapabilities(GraphicsAPI api) const;
    
    // Проверки доступности
    bool IsDirectX12Available() const;
    bool IsDirectX11Available() const;
    bool IsVulkanAvailable() const;
    bool IsANGLEAvailable() const;
    
    // Определение характеристик системы
    void DetectSystemCapabilities();
    bool IsWindows10OrGreater() const;
    bool IsWindows11OrGreater() const;
    bool HasDedicatedGPU() const;
    bool SupportsHDR() const;
};

} // namespace window_winapi
