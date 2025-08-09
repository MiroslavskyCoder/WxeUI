#include "../../include/window_winapi.h"
#include <iostream>
#include <string>

using namespace WxeUI;

class ShowcaseLayer : public ILayer {
public:
    ShowcaseLayer() : visible_(true), zOrder_(0) {}
    
    void OnRender(SkCanvas* canvas) override {
        if (!visible_) return;
        
        // Демонстрация различных возможностей рендеринга
        SkPaint paint;
        paint.setColor(SK_ColorBLUE);
        paint.setAntiAlias(true);
        
        // Рисование прямоугольника
        SkRect rect = SkRect::MakeXYWH(50, 50, 200, 100);
        canvas->drawRect(rect, paint);
        
        // Рисование текста
        paint.setColor(SK_ColorWHITE);
        SkFont font;
        font.setSize(24);
        canvas->drawString("Window WinAPI Showcase", 60, 110, font, paint);
        
        // Демонстрация градиента
        SkPoint points[2] = {{300, 50}, {500, 150}};
        SkColor colors[2] = {SK_ColorRED, SK_ColorYELLOW};
        auto shader = SkGradientShader::MakeLinear(points, colors, nullptr, 2, SkTileMode::kClamp);
        
        paint.setShader(shader);
        SkRect gradientRect = SkRect::MakeXYWH(300, 50, 200, 100);
        canvas->drawRect(gradientRect, paint);
    }
    
    void OnUpdate(float deltaTime) override {
        // Анимации и обновления
    }
    
    void OnResize(int width, int height) override {
        // Адаптация к новому размеру
    }
    
    LayerType GetType() const override { return LayerType::Content; }
    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }
    int GetZOrder() const override { return zOrder_; }
    void SetZOrder(int zOrder) override { zOrder_ = zOrder; }
    
private:
    bool visible_;
    int zOrder_;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Настройка окна
    WindowConfig config;
    config.title = L"Complete Showcase - Window WinAPI";
    config.width = 1280;
    config.height = 720;
    config.dpiAwareness = DPIAwareness::PerMonitorV2;
    
    // Создание окна
    Window window(config);
    
    if (!window.Create()) {
        MessageBoxW(nullptr, L"Не удалось создать окно!", L"Ошибка", MB_ICONERROR);
        return -1;
    }
    
    // Инициализация графики (DirectX12 для лучшей производительности)
    if (!window.InitializeGraphics(GraphicsAPI::DirectX12)) {
        // Fallback на DirectX11
        if (!window.InitializeGraphics(GraphicsAPI::DirectX11)) {
            MessageBoxW(nullptr, L"Не удалось инициализировать графику!", L"Ошибка", MB_ICONERROR);
            return -1;
        }
    }
    
    // Добавление showcase слоя
    auto showcaseLayer = std::make_shared<ShowcaseLayer>();
    window.GetLayerSystem().AddLayer(showcaseLayer);
    
    // Настройка высокочастотного рендеринга
    window.FrameHigh();
    
    // Включение OpenScreen функциональности
    window.OpenScreen("showcase_screen");
    
    // Настройка обработчиков событий
    window.OnResize = [&](int width, int height) {
        std::wcout << L"Размер окна изменен: " << width << L"x" << height << std::endl;
    };
    
    window.OnClose = [&]() {
        std::wcout << L"Закрытие окна..." << std::endl;
        PostQuitMessage(0);
    };
    
    // Настройка event system
    events::EventSystem::Subscribe<events::WindowResizeEvent>([](const events::Event& e) {
        const auto& resizeEvent = static_cast<const events::WindowResizeEvent&>(e);
        std::wcout << L"Event: Размер изменен на " << resizeEvent.GetWidth() << L"x" << resizeEvent.GetHeight() << std::endl;
    });
    
    events::EventSystem::Subscribe<events::MouseMoveEvent>([](const events::Event& e) {
        const auto& mouseEvent = static_cast<const events::MouseMoveEvent&>(e);
        // Можно добавить обработку движения мыши
    });
    
    // Показ окна
    window.Show();
    
    // Основной цикл сообщений
    MSG msg = {};
    auto lastTime = std::chrono::steady_clock::now();
    
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        // Вычисление deltaTime
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Обновление окна
        window.Update(deltaTime);
        
        // Рендеринг (если не используется FrameHigh)
        if (!window.GetFrameHigh().IsRenderingActive()) {
            window.Render();
        }
        
        // Вывод статистики производительности каждую секунду
        static auto lastStatsTime = std::chrono::steady_clock::now();
        if (currentTime - lastStatsTime > std::chrono::seconds(1)) {
            auto stats = window.GetRenderStats();
            std::wcout << L"FPS: " << stats.fps << L", Время кадра: " 
                      << std::chrono::duration<float, std::milli>(stats.frameTime).count() 
                      << L"ms" << std::endl;
            lastStatsTime = currentTime;
        }
    }
    
    return static_cast<int>(msg.wParam);
}
