#include "graphics/angle_context.h"
#include <iostream>
#include <sstream>

namespace WxeUI {

ANGLEContext::ANGLEContext() 
    : display_(EGL_NO_DISPLAY)
    , context_(EGL_NO_CONTEXT)
    , surface_(EGL_NO_SURFACE)
    , config_(nullptr)
    , preferredBackend_(Backend::D3D11)
    , currentBackend_(Backend::D3D11)
    , framebuffer_(0)
    , colorTexture_(0)
    , depthTexture_(0)
    , webOptimizationsEnabled_(false)
    , highPerformancePreferred_(false)
    , hwnd_(nullptr)
    , width_(0)
    , height_(0) {
}

ANGLEContext::~ANGLEContext() {
    Shutdown();
}

bool ANGLEContext::Initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    
    if (!InitializeEGL(hwnd)) return false;
    if (!CreateEGLContext()) return false;
    if (!CreateFramebuffer(width, height)) return false;
    if (!CreateSkiaContext()) return false;
    
    QueryWebGLInfo();
    
    return true;
}

bool ANGLEContext::InitializeEGL(HWND hwnd) {
    // Get display attributes based on preferred backend
    std::vector<EGLint> displayAttribs = GetDisplayAttributes();
    
    display_ = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, 
                                   reinterpret_cast<void*>(GetDC(hwnd)), 
                                   displayAttribs.data());
    
    if (display_ == EGL_NO_DISPLAY) {
        return false;
    }
    
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(display_, &majorVersion, &minorVersion)) {
        return false;
    }
    
    return true;
}

bool ANGLEContext::CreateEGLContext() {
    // Get config attributes
    std::vector<EGLint> configAttribs = GetConfigAttributes();
    
    EGLint numConfigs;
    if (!eglChooseConfig(display_, configAttribs.data(), &config_, 1, &numConfigs) || numConfigs == 0) {
        return false;
    }
    
    // Create surface
    EGLint surfaceAttribs[] = {
        EGL_WIDTH, width_,
        EGL_HEIGHT, height_,
        EGL_NONE
    };
    
    surface_ = eglCreateWindowSurface(display_, config_, hwnd_, surfaceAttribs);
    if (surface_ == EGL_NO_SURFACE) {
        return false;
    }
    
    // Get context attributes
    std::vector<EGLint> contextAttribs = GetContextAttributes();
    
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs.data());
    if (context_ == EGL_NO_CONTEXT) {
        return false;
    }
    
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        return false;
    }
    
    // Detect actual backend being used
    currentBackend_ = DetectBestBackend();
    
    return true;
}

bool ANGLEContext::CreateFramebuffer(int width, int height) {
    // Create color texture
    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Create depth texture
    glGenTextures(1, &depthTexture_);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Create framebuffer
    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool ANGLEContext::CreateSkiaContext() {
    // Create Skia OpenGL context
    auto interface = GrGLMakeNativeInterface();
    if (!interface) {
        return false;
    }
    
    grContext_ = GrDirectContext::MakeGL(interface);
    if (!grContext_) {
        return false;
    }
    
    UpdateSkiaSurface(width_, height_);
    return true;
}

void ANGLEContext::UpdateSkiaSurface(int width, int height) {
    GrGLTextureInfo textureInfo;
    textureInfo.fTarget = GL_TEXTURE_2D;
    textureInfo.fID = colorTexture_;
    textureInfo.fFormat = GL_RGBA8;
    
    GrBackendTexture backendTexture(width, height, GrMipMapped::kNo, textureInfo);
    
    skiaSurface_ = SkSurface::MakeFromBackendTexture(
        grContext_.get(),
        backendTexture,
        kBottomLeft_GrSurfaceOrigin,
        1,
        kRGBA_8888_SkColorType,
        nullptr,
        nullptr
    );
}

ANGLEContext::Backend ANGLEContext::DetectBestBackend() {
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::string rendererStr(renderer ? renderer : "");
    
    if (rendererStr.find("Direct3D11") != std::string::npos) {
        return Backend::D3D11;
    } else if (rendererStr.find("Direct3D9") != std::string::npos) {
        return Backend::D3D9;
    } else if (rendererStr.find("Vulkan") != std::string::npos) {
        return Backend::Vulkan;
    } else {
        return Backend::OpenGL;
    }
}

void ANGLEContext::QueryWebGLInfo() {
    webglInfo_.version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    webglInfo_.shadingLanguageVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    webglInfo_.vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    webglInfo_.renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    
    // Get extensions
    GLint numExtensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; i++) {
        const char* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (extension) {
            webglInfo_.extensions.push_back(extension);
        }
    }
    
    // Get limits
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &webglInfo_.maxTextureSize);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &webglInfo_.maxCombinedTextureImageUnits);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &webglInfo_.maxVertexAttribs);
}

void ANGLEContext::Shutdown() {
    skiaSurface_.reset();
    grContext_.reset();
    
    if (framebuffer_) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
    
    if (colorTexture_) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    
    if (depthTexture_) {
        glDeleteTextures(1, &depthTexture_);
        depthTexture_ = 0;
    }
    
    if (context_ != EGL_NO_CONTEXT) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
    }
    
    if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
    
    if (display_ != EGL_NO_DISPLAY) {
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void ANGLEContext::ResizeBuffers(int width, int height) {
    if (width_ == width && height_ == height) return;
    
    width_ = width;
    height_ = height;
    
    // Recreate framebuffer with new size
    if (framebuffer_) {
        glDeleteFramebuffers(1, &framebuffer_);
        glDeleteTextures(1, &colorTexture_);
        glDeleteTextures(1, &depthTexture_);
    }
    
    CreateFramebuffer(width, height);
    UpdateSkiaSurface(width, height);
}

void ANGLEContext::Present() {
    eglSwapBuffers(display_, surface_);
}

void ANGLEContext::Clear(float r, float g, float b, float a) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, width_, height_);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

sk_sp<SkSurface> ANGLEContext::GetSkiaSurface() {
    return skiaSurface_;
}

void ANGLEContext::WaitForGPU() {
    glFinish();
}

std::vector<EGLint> ANGLEContext::GetDisplayAttributes() const {
    std::vector<EGLint> attribs = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 11,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 1,
    };
    
    if (webOptimizationsEnabled_) {
        attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        attribs.push_back(highPerformancePreferred_ ? 
                         EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE :
                         EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
    }
    
    attribs.push_back(EGL_NONE);
    return attribs;
}

std::vector<EGLint> ANGLEContext::GetConfigAttributes() const {
    return {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
}

std::vector<EGLint> ANGLEContext::GetContextAttributes() const {
    return {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
}

ANGLEContext::WebGLInfo ANGLEContext::GetWebGLInfo() const {
    return webglInfo_;
}

bool ANGLEContext::IsWebGLCompatible() const {
    return !webglInfo_.version.empty() && 
           webglInfo_.version.find("ES") != std::string::npos;
}

void ANGLEContext::EnableWebOptimizations(bool enable) {
    webOptimizationsEnabled_ = enable;
}

void ANGLEContext::SetPowerPreference(bool preferHighPerformance) {
    highPerformancePreferred_ = preferHighPerformance;
}

} // namespace window_winapi
