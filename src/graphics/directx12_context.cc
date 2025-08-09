#include "graphics/directx12_context.h"
#include "graphics/graphics_manager.h"
#include <iostream>
#include <cassert>
#include <algorithm>

// Helper macro for checking HRESULT
#define ThrowIfFailed(hr) \
    if (FAILED(hr)) { \
        OutputDebugStringA(("FAILED: " + std::to_string(hr) + "\n").c_str()); \
        return false; \
    }

namespace WxeUI {

DirectX12Context::DirectX12Context() 
    : device_(nullptr)
    , commandQueue_(nullptr)
    , swapChain_(nullptr)
    , rtvHeap_(nullptr)
    , dsvHeap_(nullptr)
    , commandList_(nullptr)
    , fence_(nullptr)
    , fenceEvent_(nullptr)
    , currentBackBufferIndex_(0)
    , hdrSupported_(false)
    , colorSpace_(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
    , hwnd_(nullptr)
    , width_(0)
    , height_(0) {
    
    for (UINT i = 0; i < FrameCount; i++) {
        renderTargets_[i] = nullptr;
        commandAllocators_[i] = nullptr;
        fenceValues_[i] = 0;
    }
}

DirectX12Context::~DirectX12Context() {
    Shutdown();
}

bool DirectX12Context::Initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    
    // Enable debug layer in debug builds
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        
        Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)))) {
            debugController1->SetEnableGPUBasedValidation(TRUE);
        }
    }
#endif
    
    if (!CreateDevice()) return false;
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateDescriptorHeaps()) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateDepthStencil(width, height)) return false;
    if (!CreateFence()) return false;
    if (!CreateSkiaContext()) return false;
    
    InitializeHDR();
    
    return true;
}

bool DirectX12Context::CreateDevice() {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;
    
#ifdef _DEBUG
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)));
    
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0; ; ++adapterIndex) {
        hardwareAdapter = nullptr;
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &hardwareAdapter)) {
            break;
        }
        
        // Check if adapter supports DirectX 12
        if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
            break;
        }
    }
    
    if (!device_) {
        // Fallback to WARP device
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)));
    }
    
    return true;
}

bool DirectX12Context::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    
    ThrowIfFailed(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)));
    commandQueue_->SetName(L"Main Command Queue");
    
    return true;
}

bool DirectX12Context::CreateSwapChain(HWND hwnd, int width, int height) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));
    
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));
    
    // Disable fullscreen transitions
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    
    ThrowIfFailed(swapChain.As(&swapChain_));
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    
    return true;
}

bool DirectX12Context::CreateDescriptorHeaps() {
    // RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)));
    
    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_)));
    
    return true;
}

bool DirectX12Context::CreateRenderTargets() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    for (UINT n = 0; n < FrameCount; n++) {
        ThrowIfFailed(swapChain_->GetBuffer(n, IID_PPV_ARGS(&renderTargets_[n])));
        device_->CreateRenderTargetView(renderTargets_[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
        
        // Create command allocators
        ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[n])));
    }
    
    // Create command list
    ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0].Get(), nullptr, IID_PPV_ARGS(&commandList_)));
    ThrowIfFailed(commandList_->Close());
    
    return true;
}

bool DirectX12Context::CreateDepthStencil(int width, int height) {
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
    
    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;
    
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    
    ThrowIfFailed(device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&depthStencil_)
    ));
    
    device_->CreateDepthStencilView(depthStencil_.Get(), &depthStencilDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
    
    return true;
}

bool DirectX12Context::CreateFence() {
    ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent_ == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    
    return true;
}

bool DirectX12Context::CreateSkiaContext() {
    // Create Skia DirectX 12 backend context
    GrD3DBackendContext backendContext;
    backendContext.fAdapter = nullptr;  // Will be queried from device
    backendContext.fDevice = device_.Get();
    backendContext.fQueue = commandQueue_.Get();
    backendContext.fMemoryAllocator = nullptr;  // Use default
    
    grContext_ = GrDirectContext::MakeDirect3D(backendContext);
    if (!grContext_) {
        return false;
    }
    
    UpdateSkiaSurfaces();
    return true;
}

void DirectX12Context::UpdateSkiaSurfaces() {
    for (UINT i = 0; i < FrameCount; i++) {
        GrD3DTextureResourceInfo textureResourceInfo;
        textureResourceInfo.fResource = renderTargets_[i].Get();
        textureResourceInfo.fResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        textureResourceInfo.fFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureResourceInfo.fSampleCount = 1;
        textureResourceInfo.fLevelCount = 1;
        
        GrBackendTexture backendTexture(width_, height_, textureResourceInfo);
        
        skiaSurfaces_[i] = SkSurface::MakeFromBackendTexture(
            grContext_.get(),
            backendTexture,
            kTopLeft_GrSurfaceOrigin,
            1,
            kRGBA_8888_SkColorType,
            nullptr,
            nullptr
        );
    }
}

void DirectX12Context::Shutdown() {
    if (device_) {
        WaitForGPU();
    }
    
    skiaSurfaces_[0].reset();
    skiaSurfaces_[1].reset();
    grContext_.reset();
    
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    
    fence_.Reset();
    depthStencil_.Reset();
    
    for (UINT i = 0; i < FrameCount; i++) {
        renderTargets_[i].Reset();
        commandAllocators_[i].Reset();
    }
    
    commandList_.Reset();
    dsvHeap_.Reset();
    rtvHeap_.Reset();
    swapChain_.Reset();
    commandQueue_.Reset();
    device_.Reset();
}

void DirectX12Context::ResizeBuffers(int width, int height) {
    if (width_ == width && height_ == height) return;
    
    WaitForGPU();
    
    width_ = width;
    height_ = height;
    
    // Release old render targets
    for (UINT i = 0; i < FrameCount; i++) {
        renderTargets_[i].Reset();
        skiaSurfaces_[i].reset();
    }
    depthStencil_.Reset();
    
    // Resize swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChain_->GetDesc(&swapChainDesc);
    ThrowIfFailed(swapChain_->ResizeBuffers(FrameCount, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
    
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    
    // Recreate render targets and depth stencil
    CreateRenderTargets();
    CreateDepthStencil(width, height);
    UpdateSkiaSurfaces();
}

void DirectX12Context::Present() {
    ThrowIfFailed(swapChain_->Present(1, 0));
    MoveToNextFrame();
}

void DirectX12Context::Clear(float r, float g, float b, float a) {
    auto commandAllocator = commandAllocators_[currentBackBufferIndex_];
    ThrowIfFailed(commandAllocator->Reset());
    ThrowIfFailed(commandList_->Reset(commandAllocator.Get(), nullptr));
    
    // Transition render target to render target state
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets_[currentBackBufferIndex_].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList_->ResourceBarrier(1, &barrier);
    
    // Clear render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtvHandle.Offset(currentBackBufferIndex_, rtvDescriptorSize);
    
    const float clearColor[] = { r, g, b, a };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    
    // Clear depth stencil
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap_->GetCPUDescriptorHandleForHeapStart());
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    
    // Transition back to present state
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets_[currentBackBufferIndex_].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList_->ResourceBarrier(1, &barrier);
    
    ThrowIfFailed(commandList_->Close());
    
    ID3D12CommandList* ppCommandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

sk_sp<SkSurface> DirectX12Context::GetSkiaSurface() {
    return skiaSurfaces_[currentBackBufferIndex_];
}

void DirectX12Context::WaitForGPU() {
    const UINT64 fence = fenceValues_[currentBackBufferIndex_];
    ThrowIfFailed(commandQueue_->Signal(fence_.Get(), fence));
    fenceValues_[currentBackBufferIndex_]++;
    
    if (fence_->GetCompletedValue() < fence) {
        ThrowIfFailed(fence_->SetEventOnCompletion(fence, fenceEvent_));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void DirectX12Context::MoveToNextFrame() {
    const UINT64 currentFenceValue = fenceValues_[currentBackBufferIndex_];
    ThrowIfFailed(commandQueue_->Signal(fence_.Get(), currentFenceValue));
    
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    
    if (fence_->GetCompletedValue() < fenceValues_[currentBackBufferIndex_]) {
        ThrowIfFailed(fence_->SetEventOnCompletion(fenceValues_[currentBackBufferIndex_], fenceEvent_));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    
    fenceValues_[currentBackBufferIndex_] = currentFenceValue + 1;
}

DirectX12Context::GPUMemoryInfo DirectX12Context::GetMemoryInfo() const {
    GPUMemoryInfo info = {};
    
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
    if (SUCCEEDED(swapChain_->GetDevice(IID_PPV_ARGS(&adapter)))) {
        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
            info.totalMemory = memInfo.Budget;
            info.usedMemory = memInfo.CurrentUsage;
            info.availableMemory = memInfo.AvailableForReservation;
        }
    }
    
    return info;
}

bool DirectX12Context::InitializeHDR() {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
    
    if (SUCCEEDED(swapChain_->GetContainingOutput(&output)) &&
        SUCCEEDED(output.As(&output6))) {
        
        DXGI_OUTPUT_DESC1 outputDesc;
        if (SUCCEEDED(output6->GetDesc1(&outputDesc))) {
            hdrSupported_ = (outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            
            if (hdrSupported_) {
                colorSpace_ = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                swapChain_->SetColorSpace1(colorSpace_);
            }
        }
    }
    
    return hdrSupported_;
}

void DirectX12Context::SetHDRMetadata(float maxLuminance, float minLuminance) {
    if (!hdrSupported_) return;
    
    DXGI_HDR_METADATA_HDR10 hdrMetadata = {};
    hdrMetadata.RedPrimary[0] = static_cast<UINT16>(0.708 * 50000);
    hdrMetadata.RedPrimary[1] = static_cast<UINT16>(0.292 * 50000);
    hdrMetadata.GreenPrimary[0] = static_cast<UINT16>(0.170 * 50000);
    hdrMetadata.GreenPrimary[1] = static_cast<UINT16>(0.797 * 50000);
    hdrMetadata.BluePrimary[0] = static_cast<UINT16>(0.131 * 50000);
    hdrMetadata.BluePrimary[1] = static_cast<UINT16>(0.046 * 50000);
    hdrMetadata.WhitePoint[0] = static_cast<UINT16>(0.3127 * 50000);
    hdrMetadata.WhitePoint[1] = static_cast<UINT16>(0.3290 * 50000);
    hdrMetadata.MaxMasteringLuminance = static_cast<UINT>(maxLuminance * 10000);
    hdrMetadata.MinMasteringLuminance = static_cast<UINT>(minLuminance * 0.0001);
    hdrMetadata.MaxContentLightLevel = static_cast<UINT16>(maxLuminance);
    hdrMetadata.MaxFrameAverageLightLevel = static_cast<UINT16>(maxLuminance * 0.5);
    
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain4;
    if (SUCCEEDED(swapChain_.As(&swapChain4))) {
        swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdrMetadata), &hdrMetadata);
    }
}

} // namespace window_winapi
