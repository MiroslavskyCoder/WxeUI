#include "graphics/directx11_context.h"
#include <iostream>
#include <thread>

#define ThrowIfFailed(hr) \
    if (FAILED(hr)) { \
        OutputDebugStringA(("FAILED: " + std::to_string(hr) + "\n").c_str()); \
        return false; \
    }

namespace WxeUI {

DirectX11Context::DirectX11Context() 
    : device_(nullptr)
    , immediateContext_(nullptr)
    , deferredContext_(nullptr)
    , swapChain_(nullptr)
    , renderTargetView_(nullptr)
    , depthStencilView_(nullptr)
    , backBuffer_(nullptr)
    , depthStencilBuffer_(nullptr)
    , featureLevel_(D3D_FEATURE_LEVEL_11_0)
    , hwnd_(nullptr)
    , width_(0)
    , height_(0) {
}

DirectX11Context::~DirectX11Context() {
    Shutdown();
}

bool DirectX11Context::Initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    
    if (!CreateDevice()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRenderTargetView()) return false;
    if (!CreateDepthStencilView(width, height)) return false;
    if (!CreateSkiaContext()) return false;
    
    CheckCompatibility();
    
    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    immediateContext_->RSSetViewports(1, &viewport);
    
    return true;
}

bool DirectX11Context::CreateDevice() {
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    
    ThrowIfFailed(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel_,
        &immediateContext_
    ));
    
    // Create deferred context for multi-threading
    ThrowIfFailed(device_->CreateDeferredContext(0, &deferredContext_));
    
    return true;
}

bool DirectX11Context::CreateSwapChain(HWND hwnd, int width, int height) {
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    
    ThrowIfFailed(device_.As(&dxgiDevice));
    ThrowIfFailed(dxgiDevice->GetAdapter(&dxgiAdapter));
    ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));
    
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    
    ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
        device_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain_
    ));
    
    // Disable fullscreen transitions
    ThrowIfFailed(dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    
    return true;
}

bool DirectX11Context::CreateRenderTargetView() {
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer_)));
    ThrowIfFailed(device_->CreateRenderTargetView(backBuffer_.Get(), nullptr, &renderTargetView_));
    
    return true;
}

bool DirectX11Context::CreateDepthStencilView(int width, int height) {
    D3D11_TEXTURE2D_DESC depthStencilDesc = {};
    depthStencilDesc.Width = width;
    depthStencilDesc.Height = height;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    
    ThrowIfFailed(device_->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer_));
    ThrowIfFailed(device_->CreateDepthStencilView(depthStencilBuffer_.Get(), nullptr, &depthStencilView_));
    
    return true;
}

bool DirectX11Context::CreateSkiaContext() {
    // Create Skia DirectX 11 backend context
    GrD3DBackendContext backendContext;
    backendContext.fAdapter = nullptr;
    backendContext.fDevice = device_.Get();
    
    grContext_ = GrDirectContext::MakeDirect3D(backendContext);
    if (!grContext_) {
        return false;
    }
    
    UpdateSkiaSurface();
    return true;
}

void DirectX11Context::UpdateSkiaSurface() {
    Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
    if (SUCCEEDED(backBuffer_.As(&dxgiResource))) {
        HANDLE sharedHandle;
        if (SUCCEEDED(dxgiResource->GetSharedHandle(&sharedHandle))) {
            GrD3DTextureResourceInfo textureResourceInfo;
            textureResourceInfo.fResource = backBuffer_.Get();
            textureResourceInfo.fResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            textureResourceInfo.fFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureResourceInfo.fSampleCount = 1;
            textureResourceInfo.fLevelCount = 1;
            
            GrBackendTexture backendTexture(width_, height_, textureResourceInfo);
            
            skiaSurface_ = SkSurface::MakeFromBackendTexture(
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
}

void DirectX11Context::CheckCompatibility() {
    compatInfo_.featureLevel = featureLevel_;
    
    // Check tessellation support
    compatInfo_.supportsTessellation = (featureLevel_ >= D3D_FEATURE_LEVEL_11_0);
    
    // Check compute shader support
    compatInfo_.supportsComputeShaders = (featureLevel_ >= D3D_FEATURE_LEVEL_11_0);
    
    // Check multi-threaded resource support
    D3D11_FEATURE_DATA_THREADING threadingSupport;
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingSupport, sizeof(threadingSupport)))) {
        compatInfo_.supportsMultithreadedResources = threadingSupport.DriverConcurrentCreates;
    }
    
    // Get max texture size
    switch (featureLevel_) {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            compatInfo_.maxTexture2DSize = 16384;
            break;
        case D3D_FEATURE_LEVEL_10_1:
        case D3D_FEATURE_LEVEL_10_0:
            compatInfo_.maxTexture2DSize = 8192;
            break;
        default:
            compatInfo_.maxTexture2DSize = 4096;
            break;
    }
}

void DirectX11Context::Shutdown() {
    // Clean up deferred contexts
    std::lock_guard<std::mutex> lock(contextsMutex_);
    deferredContexts_.clear();
    
    skiaSurface_.reset();
    grContext_.reset();
    
    if (immediateContext_) {
        immediateContext_->ClearState();
    }
    
    depthStencilView_.Reset();
    depthStencilBuffer_.Reset();
    renderTargetView_.Reset();
    backBuffer_.Reset();
    swapChain_.Reset();
    deferredContext_.Reset();
    immediateContext_.Reset();
    device_.Reset();
}

void DirectX11Context::ResizeBuffers(int width, int height) {
    if (width_ == width && height_ == height) return;
    
    width_ = width;
    height_ = height;
    
    // Clear state and release old buffers
    immediateContext_->OMSetRenderTargets(0, nullptr, nullptr);
    skiaSurface_.reset();
    renderTargetView_.Reset();
    depthStencilView_.Reset();
    backBuffer_.Reset();
    depthStencilBuffer_.Reset();
    
    // Resize swap chain
    ThrowIfFailed(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));
    
    // Recreate render target and depth stencil
    CreateRenderTargetView();
    CreateDepthStencilView(width, height);
    UpdateSkiaSurface();
    
    // Update viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    immediateContext_->RSSetViewports(1, &viewport);
}

void DirectX11Context::Present() {
    swapChain_->Present(1, 0);
}

void DirectX11Context::Clear(float r, float g, float b, float a) {
    const float clearColor[] = { r, g, b, a };
    immediateContext_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    immediateContext_->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    
    // Set render targets
    immediateContext_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());
}

sk_sp<SkSurface> DirectX11Context::GetSkiaSurface() {
    return skiaSurface_;
}

void DirectX11Context::WaitForGPU() {
    // DirectX 11 doesn't have explicit GPU synchronization like DirectX 12
    // Flush the immediate context
    immediateContext_->Flush();
}

std::shared_ptr<DirectX11Context::DeferredContext> DirectX11Context::CreateDeferredContext() {
    auto deferredCtx = std::make_shared<DeferredContext>();
    
    HRESULT hr = device_->CreateDeferredContext(0, &deferredCtx->context);
    if (FAILED(hr)) {
        return nullptr;
    }
    
    deferredCtx->threadId = std::this_thread::get_id();
    
    std::lock_guard<std::mutex> lock(contextsMutex_);
    deferredContexts_.push_back(deferredCtx);
    
    return deferredCtx;
}

void DirectX11Context::ExecuteCommandList(std::shared_ptr<DeferredContext> context) {
    if (!context || !context->context || !context->commandList) {
        return;
    }
    
    immediateContext_->ExecuteCommandList(context->commandList.Get(), FALSE);
    context->commandList.Reset();
}

DirectX11Context::CompatibilityInfo DirectX11Context::GetCompatibilityInfo() const {
    return compatInfo_;
}

} // namespace window_winapi
