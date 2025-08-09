#include "../../include/window_winapi.h"
#include "../../include/graphics/graphics_manager.h"
#include <iostream>
#include <chrono>

using namespace WxeUI;

class DirectX12Demo {
public:
    bool Initialize() {
        // Создание окна с DirectX 12
        WindowConfig config;
        config.title = L"DirectX 12 - Продвинутый рендеринг";
        config.width = 1920;
        config.height = 1080;
        config.dpiAwareness = DPIAwareness::PerMonitorV2;
        
        window_ = std::make_unique<Window>(config);
        if (!window_->Create()) {
            std::wcout << L"Ошибка создания окна" << std::endl;
            return false;
        }
        
        // Инициализация GraphicsManager с принудительным DirectX 12
        graphicsManager_ = std::make_unique<GraphicsManager>();
        if (!graphicsManager_->Initialize(window_->GetHandle(), config.width, config.height, GraphicsAPI::DirectX12)) {
            std::wcout << L"Ошибка инициализации DirectX 12" << std::endl;
            return false;
        }
        
        // Настройка колбэков
        window_->OnRender = [this](SkCanvas* canvas) { Render(canvas); };
        window_->OnUpdate = [this](float deltaTime) { Update(deltaTime); };
        window_->OnResize = [this](int width, int height) { 
            graphicsManager_->ResizeBuffers(width, height); 
        };
        
        // Включение мониторинга производительности
        graphicsManager_->StartPerformanceMonitoring();
        graphicsManager_->OnPerformanceUpdate = [](const GraphicsManager::PerformanceMetrics& metrics) {
            static auto lastUpdate = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - lastUpdate > std::chrono::seconds(1)) {
                std::wcout << L"FPS: " << metrics.fps << L", Frame Time: " << metrics.frameTime << L"ms" << std::endl;
                lastUpdate = now;
            }
        };
        
        // HDR настройки
        auto* dx12Context = static_cast<DirectX12Context*>(graphicsManager_->GetCurrentAPI() == GraphicsAPI::DirectX12 ? 
                                                           graphicsManager_.get() : nullptr);
        if (dx12Context && dx12Context->IsHDRSupported()) {
            dx12Context->SetHDRMetadata(1000.0f, 0.0001f);
            std::wcout << L"HDR включен" << std::endl;
        }
        
        return true;
    }
    
    void Run() {
        window_->Show();
        
        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
private:
    void Update(float deltaTime) {
        rotation_ += deltaTime * 45.0f; // 45 градусов в секунду
        if (rotation_ > 360.0f) rotation_ -= 360.0f;
    }
    
    void Render(SkCanvas* canvas) {
        if (!canvas) return;
        
        // Очистка с градиентом
        float time = rotation_ / 360.0f;
        float r = 0.1f + 0.1f * sin(time * 3.14159f * 2.0f);
        float g = 0.1f + 0.1f * sin(time * 3.14159f * 2.0f + 2.0f);
        float b = 0.1f + 0.1f * sin(time * 3.14159f * 2.0f + 4.0f);
        
        graphicsManager_->Clear(r, g, b, 1.0f);
        
        // Демонстрация продвинутого рендеринга
        canvas->save();
        
        // Центрирование
        SkRect bounds = canvas->getLocalClipBounds();
        canvas->translate(bounds.centerX(), bounds.centerY());
        canvas->rotate(rotation_);
        
        // Рисование сложной геометрии
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(3.0f);
        
        // Множественные кольца с разными цветами
        for (int i = 0; i < 10; i++) {
            float radius = 50.0f + i * 30.0f;
            float hue = (rotation_ + i * 36.0f) / 360.0f;
            SkColor color = SkHSVToColor(hue * 360.0f, 1.0f, 1.0f);
            paint.setColor(color);
            
            canvas->drawCircle(0, 0, radius, paint);
        }
        
        canvas->restore();
        
        // Текст с информацией о DirectX 12
        SkPaint textPaint;
        textPaint.setColor(SK_ColorWHITE);
        textPaint.setAntiAlias(true);
        
        SkFont font;
        font.setSize(24);
        
        canvas->drawString("DirectX 12 - Продвинутый рендеринг", 20, 40, font, textPaint);
        
        auto metrics = graphicsManager_->GetPerformanceMetrics();
        std::string fpsText = "FPS: " + std::to_string((int)metrics.fps);
        canvas->drawString(fpsText.c_str(), 20, 70, font, textPaint);
        
        graphicsManager_->Present();
    }
    
    std::unique_ptr<Window> window_;
    std::unique_ptr<GraphicsManager> graphicsManager_;
    float rotation_ = 0.0f;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    DirectX12Demo demo;
    
    if (!demo.Initialize()) {
        MessageBoxW(nullptr, L"Не удалось инициализировать DirectX 12 демо", L"Ошибка", MB_ICONERROR);
        return -1;
    }
    
    demo.Run();
    return 0;
}
