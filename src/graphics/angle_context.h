#pragma once

#include "window_winapi.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <memory>
#include <string>

namespace WxeUI {

class ANGLEContext : public IGraphicsContext {
public:
    ANGLEContext();
    virtual ~ANGLEContext();
    
    bool Initialize(HWND hwnd, int width, int height) override;
    void Shutdown() override;
    void ResizeBuffers(int width, int height) override;
    void Present() override;
    void Clear(float r, float g, float b, float a) override;
    GraphicsAPI GetAPI() const override { return GraphicsAPI::ANGLE; }
    sk_sp<SkSurface> GetSkiaSurface() override;
    void WaitForGPU() override;
    
    // ANGLE специфичные методы
    EGLDisplay GetDisplay() const { return display_; }
    EGLContext GetContext() const { return context_; }
    EGLSurface GetSurface() const { return surface_; }
    
    // Backend конфигурация
    enum class Backend {
        D3D11,
        D3D9,
        OpenGL,
        Vulkan,
        Metal  // для будущей поддержки
    };
    
    void SetPreferredBackend(Backend backend) { preferredBackend_ = backend; }
    Backend GetCurrentBackend() const { return currentBackend_; }
    
    // WebGL совместимость
    struct WebGLInfo {
        std::string version;
        std::string shadingLanguageVersion;
        std::string vendor;
        std::string renderer;
        std::vector<std::string> extensions;
        int maxTextureSize;
        int maxCombinedTextureImageUnits;
        int maxVertexAttribs;
    };
    
    WebGLInfo GetWebGLInfo() const;
    bool IsWebGLCompatible() const;
    
    // Оптимизации для веб-приложений
    void EnableWebOptimizations(bool enable);
    void SetPowerPreference(bool preferHighPerformance);
    
private:
    // EGL objects
    EGLDisplay display_;
    EGLContext context_;
    EGLSurface surface_;
    EGLConfig config_;
    
    // Backend configuration
    Backend preferredBackend_;
    Backend currentBackend_;
    
    // OpenGL ES objects
    GLuint framebuffer_;
    GLuint colorTexture_;
    GLuint depthTexture_;
    
    // Skia integration
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
    
    // Web optimizations
    bool webOptimizationsEnabled_;
    bool highPerformancePreferred_;
    
    // WebGL compatibility
    WebGLInfo webglInfo_;
    
    // Helper methods
    bool InitializeEGL(HWND hwnd);
    bool CreateEGLContext();
    bool CreateFramebuffer(int width, int height);
    bool CreateSkiaContext();
    void UpdateSkiaSurface(int width, int height);
    Backend DetectBestBackend();
    void QueryWebGLInfo();
    
    // EGL attribute helpers
    std::vector<EGLint> GetDisplayAttributes() const;
    std::vector<EGLint> GetConfigAttributes() const;
    std::vector<EGLint> GetContextAttributes() const;
    
    HWND hwnd_;
    int width_, height_;
};

} // namespace window_winapi
