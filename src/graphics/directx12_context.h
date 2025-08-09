#pragma once

#include "window_winapi.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace WxeUI {

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
    
    // DirectX 12 специфичные методы
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    UINT GetCurrentBackBufferIndex() const { return currentBackBufferIndex_; }
    
    // GPU Memory Management
    struct GPUMemoryInfo {
        SIZE_T totalMemory;
        SIZE_T usedMemory;
        SIZE_T availableMemory;
        UINT adapterIndex;
    };
    
    GPUMemoryInfo GetMemoryInfo() const;
    void FlushGPU();
    void ResetCommandList();
    
    // HDR и High DPI поддержка
    bool InitializeHDR();
    void SetHDRMetadata(float maxLuminance, float minLuminance);
    bool IsHDRSupported() const { return hdrSupported_; }
    
private:
    static const UINT FrameCount = 2;
    
    // Core DirectX 12 objects
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    
    // Synchronization
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_;
    UINT64 fenceValues_[FrameCount];
    UINT currentBackBufferIndex_;
    
    // GPU Memory Allocator
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> allocator_;
    
    // Skia integration
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurfaces_[FrameCount];
    
    // HDR support
    bool hdrSupported_;
    DXGI_COLOR_SPACE_TYPE colorSpace_;
    
    // Helper methods
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateDescriptorHeaps();
    bool CreateRenderTargets();
    bool CreateDepthStencil(int width, int height);
    bool CreateFence();
    bool CreateSkiaContext();
    void UpdateSkiaSurfaces();
    void MoveToNextFrame();
    
    HWND hwnd_;
    int width_, height_;
};

} // namespace window_winapi
