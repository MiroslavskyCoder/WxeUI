#pragma once

#ifndef WINDOW_WINAPI_H
#define WINDOW_WINAPI_H

#include <windows.h>
#include <shellscalingapi.h>
#include <d3d12.h>
#include <d3d11.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>

// Подключение Skia
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"

// Подключение новых компонентов
#include "graphics/graphics_manager.h"
#include "memory/memory_manager.h"
#include "rendering/quality_manager.h"
#include "rendering/performance_monitor.h"
#include "features/openscreen.h"
#include "events/event_system.h"

namespace WxeUI {

// Перечисления
enum class GraphicsAPI {
    DirectX12,
    DirectX11,
    Vulkan,
    ANGLE,
    Software
};

enum class DPIAwareness {
    Unaware,
    System,
    PerMonitor,
    PerMonitorV2
};

enum class LayerType {
    Background,
    Content,
    UI,
    Overlay,
    Popup
};

// Структуры
struct WindowConfig {
    std::wstring title = L"Window WinAPI";
    int width = 1280;
    int height = 720;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
    bool resizable = true;
    bool maximizable = true;
    bool minimizable = true;
    DPIAwareness dpiAwareness = DPIAwareness::PerMonitorV2;
};

struct DisplayInfo {
    HMONITOR monitor;
    RECT workArea;
    RECT monitorArea;
    float dpiX;
    float dpiY;
    float scaleFactor;
};

struct RenderStats {
    std::chrono::nanoseconds frameTime;
    std::chrono::nanoseconds cpuTime;
    std::chrono::nanoseconds gpuTime;
    uint64_t frameCount;
    float fps;
    size_t memoryUsage;
};

// Интерфейсы
class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;
    virtual bool Initialize(HWND hwnd, int width, int height) = 0;
    virtual void Shutdown() = 0;
    virtual void ResizeBuffers(int width, int height) = 0;
    virtual void Present() = 0;
    virtual void Clear(float r, float g, float b, float a) = 0;
    virtual GraphicsAPI GetAPI() const = 0;
    virtual sk_sp<SkSurface> GetSkiaSurface() = 0;
    virtual void WaitForGPU() = 0;
};

class ILayer {
public:
    virtual ~ILayer() = default;
    virtual void OnRender(SkCanvas* canvas) = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnResize(int width, int height) = 0;
    virtual LayerType GetType() const = 0;
    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual int GetZOrder() const = 0;
    virtual void SetZOrder(int zOrder) = 0;
};

// Основные классы
class DPIHelper {
public:
    static bool SetDPIAwareness(DPIAwareness awareness);
    static float GetDPIScale(HWND hwnd);
    static void GetDPI(HWND hwnd, float* dpiX, float* dpiY);
    static RECT ScaleRect(const RECT& rect, float scale);
    static DisplayInfo GetDisplayInfo(HWND hwnd);
    static void AdjustWindowRectForDPI(RECT* rect, DWORD style, DWORD exStyle, HWND hwnd);
};

class LayerSystem {
public:
    void AddLayer(std::shared_ptr<ILayer> layer);
    void RemoveLayer(std::shared_ptr<ILayer> layer);
    void RenderLayers(SkCanvas* canvas);
    void UpdateLayers(float deltaTime);
    void ResizeLayers(int width, int height);
    void SortLayers();
    std::vector<std::shared_ptr<ILayer>> GetLayers() const;
    
private:
    std::vector<std::shared_ptr<ILayer>> layers_;
    bool needsSort_ = false;
};

class FragmentCache {
public:
    struct CacheEntry {
        sk_sp<SkSurface> surface;
        std::chrono::steady_clock::time_point lastUsed;
        size_t hash;
        bool isDirty;
    };
    
    sk_sp<SkSurface> GetCachedSurface(const std::string& key, int width, int height);
    void InvalidateCache(const std::string& key);
    void ClearCache();
    void SetMaxCacheSize(size_t maxSize);
    void GarbageCollect();
    
private:
    std::unordered_map<std::string, CacheEntry> cache_;
    size_t maxCacheSize_ = 100;
    std::chrono::minutes maxAge_{10};
};

class SkiaCanvas {
public:
    SkiaCanvas(sk_sp<SkSurface> surface);
    ~SkiaCanvas();
    
    SkCanvas* GetCanvas() const { return canvas_; }
    sk_sp<SkSurface> GetSurface() const { return surface_; }
    
    // Высокоуровневые функции рисования
    void Clear(SkColor color);
    void DrawRect(const SkRect& rect, const SkPaint& paint);
    void DrawRoundRect(const SkRRect& rrect, const SkPaint& paint);
    void DrawText(const std::string& text, float x, float y, const SkFont& font, const SkPaint& paint);
    void DrawImage(sk_sp<SkImage> image, float x, float y, const SkPaint* paint = nullptr);
    
    // Фрагментирование и кэширование
    void BeginFragment(const std::string& fragmentId);
    void EndFragment();
    bool IsFragmentCached(const std::string& fragmentId) const;
    
private:
    sk_sp<SkSurface> surface_;
    SkCanvas* canvas_;
    FragmentCache* cache_;
    std::string currentFragment_;
};

class Window {
public:
    Window(const WindowConfig& config = {});
    virtual ~Window();
    
    // Основные операции с окном
    bool Create();
    void Destroy();
    bool IsValid() const { return hwnd_ != nullptr; }
    
    // Настройка графического контекста
    bool InitializeGraphics(GraphicsAPI api);
    void SetGraphicsContext(std::unique_ptr<IGraphicsContext> context);
    
    // Основной цикл
    void Show();
    void Hide();
    void Minimize();
    void Maximize();
    void Restore();
    void Close();
    
    // События
    std::function<void(int, int)> OnResize;
    std::function<void()> OnClose;
    std::function<void(SkCanvas*)> OnRender;
    std::function<void(float)> OnUpdate;
    std::function<void(int, int, UINT)> OnMouseMove;
    std::function<void(int, UINT)> OnMouseButton;
    std::function<void(UINT, WPARAM)> OnKeyboard;
    std::function<void(float)> OnDPIChanged;
    
    // Getters
    HWND GetHandle() const { return hwnd_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    float GetDPIScale() const { return dpiScale_; }
    DisplayInfo GetDisplayInfo() const;
    RenderStats GetRenderStats() const { return renderStats_; }
    
    // Layer system
    LayerSystem& GetLayerSystem() { return layerSystem_; }
    
    // Advanced rendering functions
    void OpenScreen(const std::string& screenName);
    void FrameHigh();
    sk_sp<SkSurface> ToFrame(int width, int height);
    
    // Advanced features
    features::OpenScreen& GetOpenScreen() { return openScreen_; }
    features::FrameHigh& GetFrameHigh() { return frameHigh_; }
    features::MultiMonitorSupport& GetMultiMonitorSupport() { return multiMonitor_; }
    
    // Graphics management
    graphics::GraphicsManager& GetGraphicsManager() { return graphicsManager_; }
    memory::MemoryManager& GetMemoryManager() { return memoryManager_; }
    rendering::QualityManager& GetQualityManager() { return qualityManager_; }
    rendering::PerformanceMonitor& GetPerformanceMonitor() { return performanceMonitor_; }
    
    // Event system
    void EnableEventSystem(bool enable = true);
    bool IsEventSystemEnabled() const { return eventSystemEnabled_; }
    
protected:
    virtual LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void UpdateDPI();
    void UpdateRenderStats();
    void Render();
    void Update(float deltaTime);
    
    HWND hwnd_;
    WindowConfig config_;
    int width_, height_;
    float dpiScale_;
    bool isVisible_;
    
    std::unique_ptr<IGraphicsContext> graphicsContext_;
    LayerSystem layerSystem_;
    FragmentCache fragmentCache_;
    
    RenderStats renderStats_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    
    // Advanced systems
    features::OpenScreen openScreen_;
    features::FrameHigh frameHigh_;
    features::MultiMonitorSupport multiMonitor_;
    
    graphics::GraphicsManager graphicsManager_;
    memory::MemoryManager memoryManager_;
    rendering::QualityManager qualityManager_;
    rendering::PerformanceMonitor performanceMonitor_;
    
    bool eventSystemEnabled_ = false;
    
    static const wchar_t* ClassName;
    static bool classRegistered_;
};

// Специализированные графические контексты
class DirectX12Context : public IGraphicsContext {
public:
    DirectX12Context();
    virtual ~DirectX12Context();
    
    bool Initialize(HWND hwnd, int width, int height) override;
    void Shutdown() override;
    void ResizeBuffers(int width, int height) override;
    void Present() override;
    void Clear(float r, float g, float b, float a) override;
    GraphicsAPI GetAPI() const override { return GraphicsAPI::DirectX12; }
    sk_sp<SkSurface> GetSkiaSurface() override;
    void WaitForGPU() override;
    sk_sp<GrDirectContext> GetGrContext() override { return grContext_; }
    bool SupportsHDR() const override;
    bool SupportsWideColorGamut() const override;
    
private:
    // DirectX 12 специфичные члены
    ID3D12Device* device_;
    ID3D12CommandQueue* commandQueue_;
    IDXGISwapChain3* swapChain_;
    ID3D12DescriptorHeap* rtvHeap_;
    ID3D12Resource* renderTargets_[2];
    ID3D12CommandAllocator* commandAllocator_;
    ID3D12GraphicsCommandList* commandList_;
    ID3D12Fence* fence_;
    HANDLE fenceEvent_;
    UINT64 fenceValue_;
    
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
};

class DirectX11Context : public IGraphicsContext {
public:
    DirectX11Context();
    virtual ~DirectX11Context();
    
    bool Initialize(HWND hwnd, int width, int height) override;
    void Shutdown() override;
    void ResizeBuffers(int width, int height) override;
    void Present() override;
    void Clear(float r, float g, float b, float a) override;
    GraphicsAPI GetAPI() const override { return GraphicsAPI::DirectX11; }
    sk_sp<SkSurface> GetSkiaSurface() override;
    void WaitForGPU() override;
    sk_sp<GrDirectContext> GetGrContext() override { return grContext_; }
    bool SupportsHDR() const override;
    bool SupportsWideColorGamut() const override;
    
private:
    ID3D11Device* device_;
    ID3D11DeviceContext* deviceContext_;
    IDXGISwapChain* swapChain_;
    ID3D11RenderTargetView* renderTargetView_;
    
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
};

class VulkanContext : public IGraphicsContext {
public:
    VulkanContext();
    virtual ~VulkanContext();
    
    bool Initialize(HWND hwnd, int width, int height) override;
    void Shutdown() override;
    void ResizeBuffers(int width, int height) override;
    void Present() override;
    void Clear(float r, float g, float b, float a) override;
    GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
    sk_sp<SkSurface> GetSkiaSurface() override;
    void WaitForGPU() override;
    sk_sp<GrDirectContext> GetGrContext() override { return grContext_; }
    bool SupportsHDR() const override;
    bool SupportsWideColorGamut() const override;
    
private:
    VkInstance instance_;
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapChain_;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
};

// Утилитарные функции
namespace utils {
    std::string WStringToString(const std::wstring& wstr);
    std::wstring StringToWString(const std::string& str);
    bool IsWindows10OrGreater();
    bool IsWindows11OrGreater();
    RECT GetMonitorWorkArea(HMONITOR monitor);
    std::vector<DisplayInfo> EnumerateDisplays();
}

} // namespace window_winapi

#endif // WINDOW_WINAPI_H
