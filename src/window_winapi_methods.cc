#include "window_winapi.h"

namespace WxeUI {

// Дополнительные методы Window
void Window::EnableEventSystem(bool enable) {
    eventSystemEnabled_ = enable;
    
    if (enable) {
        events::EventSystem::GetDispatcher().StartProcessing();
    } else {
        events::EventSystem::GetDispatcher().StopProcessing();
    }
}

void Window::OpenScreen(const std::string& screenName) {
    features::OpenScreen::ScreenConfig config;
    config.name = screenName;
    config.width = width_;
    config.height = height_;
    config.enableHDR = graphicsContext_ && graphicsContext_->SupportsHDR();
    config.enableWideColorGamut = graphicsContext_ && graphicsContext_->SupportsWideColorGamut();
    
    openScreen_.CreateScreen(screenName, config);
}

void Window::FrameHigh() {
    features::FrameHigh::RenderConfig config;
    config.targetFPS = 120;
    config.maxFPS = 240;
    config.adaptiveRefreshRate = true;
    
    frameHigh_.SetRenderConfig(config);
    frameHigh_.StartHighFrequencyRendering();
}

sk_sp<SkSurface> Window::ToFrame(int width, int height) {
    if (!graphicsContext_) {
        return nullptr;
    }
    
    // Создание off-screen surface для экспорта кадра
    auto grContext = graphicsContext_->GetGrContext();
    if (!grContext) {
        return nullptr;
    }
    
    return SkSurface::MakeRenderTarget(
        grContext,
        SkBudgeted::kYes,
        SkImageInfo::MakeN32Premul(width, height)
    );
}

void Window::Minimize() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
}

void Window::Maximize() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_MAXIMIZE);
    }
}

void Window::Restore() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_RESTORE);
    }
}

void Window::Close() {
    if (hwnd_) {
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    }
}

void Window::Update(float deltaTime) {
    // Обновление слоев
    layerSystem_.UpdateLayers(deltaTime);
    
    // Обновление систем производительности
    performanceMonitor_.Update(deltaTime);
    qualityManager_.Update(deltaTime);
    
    // Очистка памяти если необходимо
    memoryManager_.Update();
    
    // Уведомление через event system
    if (eventSystemEnabled_) {
        events::EventSystem::Dispatch(
            std::make_unique<events::UpdateEvent>(deltaTime)
        );
    }
    
    if (OnUpdate) {
        OnUpdate(deltaTime);
    }
}

// Дополнение метода Render
void Window::Render() {
    if (!graphicsContext_) {
        return;
    }
    
    // Получение Skia surface
    sk_sp<SkSurface> surface = graphicsContext_->GetSkiaSurface();
    if (!surface) {
        return;
    }
    
    SkCanvas* canvas = surface->getCanvas();
    if (!canvas) {
        return;
    }
    
    // Начало кадра для PerformanceMonitor
    performanceMonitor_.BeginFrame();
    
    // Очистка canvas с учетом качества
    float quality = qualityManager_.GetCurrentQuality();
    canvas->clear(SK_ColorBLACK);
    
    // Установка качества рендеринга
    SkPaint::Hinting hinting = quality > 0.8f ? SkPaint::kFull_Hinting : SkPaint::kSlight_Hinting;
    
    // Рендеринг слоев
    layerSystem_.RenderLayers(canvas);
    
    // Уведомление через event system
    if (eventSystemEnabled_) {
        events::EventSystem::DispatchImmediate(
            std::make_unique<events::RenderEvent>(canvas)
        );
    }
    
    // Пользовательский рендеринг
    if (OnRender) {
        OnRender(canvas);
    }
    
    // Завершение кадра
    performanceMonitor_.EndFrame();
    
    // Презентация
    graphicsContext_->Present();
    
    // Обновление статистики
    UpdateRenderStats();
}

} // namespace window_winapi
