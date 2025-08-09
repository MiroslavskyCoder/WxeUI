#pragma once

#include "window_winapi.h"
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace WxeUI {

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
    
    // DirectX 11 специфичные методы
    ID3D11Device* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext* GetImmediateContext() const { return immediateContext_.Get(); }
    ID3D11DeviceContext* GetDeferredContext() const { return deferredContext_.Get(); }
    
    // Поддержка Deferred Contexts для многопоточности
    struct DeferredContext {
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        Microsoft::WRL::ComPtr<ID3D11CommandList> commandList;
        std::thread::id threadId;
    };
    
    std::shared_ptr<DeferredContext> CreateDeferredContext();
    void ExecuteCommandList(std::shared_ptr<DeferredContext> context);
    
    // Обратная совместимость
    struct CompatibilityInfo {
        D3D_FEATURE_LEVEL featureLevel;
        bool supportsTessellation;
        bool supportsComputeShaders;
        bool supportsMultithreadedResources;
        UINT maxTexture2DSize;
    };
    
    CompatibilityInfo GetCompatibilityInfo() const;
    
private:
    // Core DirectX 11 objects
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferredContext_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer_;
    
    // Multi-threading support
    std::vector<std::shared_ptr<DeferredContext>> deferredContexts_;
    std::mutex contextsMutex_;
    
    // Skia integration
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
    
    // Compatibility
    D3D_FEATURE_LEVEL featureLevel_;
    CompatibilityInfo compatInfo_;
    
    // Helper methods
    bool CreateDevice();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTargetView();
    bool CreateDepthStencilView(int width, int height);
    bool CreateSkiaContext();
    void UpdateSkiaSurface();
    void CheckCompatibility();
    
    HWND hwnd_;
    int width_, height_;
};

} // namespace window_winapi
