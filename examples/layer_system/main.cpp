#include "../../include/window_winapi.h"
#include <iostream>
#include <memory>

using namespace WxeUI;

// Пример фонового слоя
class BackgroundLayer : public ILayer {
public:
    void OnRender(SkCanvas* canvas) override {
        if (!canvas) return;
        
        // Градиентный фон
        SkPaint paint;
        SkPoint points[2] = {
            SkPoint::Make(0, 0),
            SkPoint::Make(0, 600)
        };
        SkColor colors[2] = { 
            SkColorSetARGB(255, 20, 20, 30),
            SkColorSetARGB(255, 40, 40, 60)
        };
        paint.setShader(SkGradientShader::MakeLinear(points, colors, nullptr, 2, SkTileMode::kClamp));
        
        canvas->drawPaint(paint);
    }
    
    void OnUpdate(float deltaTime) override {
        // Анимация фона (опционально)
    }
    
    void OnResize(int width, int height) override {
        width_ = width;
        height_ = height;
    }
    
    LayerType GetType() const override { return LayerType::Background; }
    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }
    int GetZOrder() const override { return zOrder_; }
    void SetZOrder(int zOrder) override { zOrder_ = zOrder; }
    
private:
    bool visible_ = true;
    int zOrder_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Пример UI слоя
class UILayer : public ILayer {
public:
    UILayer() : animationTime_(0.0f) {}
    
    void OnRender(SkCanvas* canvas) override {
        if (!canvas) return;
        
        // Анимированная панель
        float offset = sin(animationTime_) * 20.0f;
        
        SkPaint panelPaint;
        panelPaint.setColor(SkColorSetARGB(200, 60, 60, 80));
        panelPaint.setAntiAlias(true);
        
        SkRect panelRect = SkRect::MakeXYWH(50 + offset, 50, 300, 200);
        SkRRect roundRect = SkRRect::MakeRectXY(panelRect, 10, 10);
        canvas->drawRRect(roundRect, panelPaint);
        
        // Заголовок
        SkPaint textPaint;
        textPaint.setColor(SK_ColorWHITE);
        textPaint.setAntiAlias(true);
        
        SkFont titleFont;
        titleFont.setSize(24);
        canvas->drawString("Система слоев", 70 + offset, 90, titleFont, textPaint);
        
        // Описание
        SkFont descFont;
        descFont.setSize(16);
        canvas->drawString("Этот слой анимируется", 70 + offset, 120, descFont, textPaint);
        canvas->drawString("и рендерится поверх фона", 70 + offset, 145, descFont, textPaint);
        
        // Индикатор времени
        std::string timeStr = "Время: " + std::to_string(static_cast<int>(animationTime_));
        canvas->drawString(timeStr.c_str(), 70 + offset, 180, descFont, textPaint);
        
        // Кнопка
        SkPaint buttonPaint;
        buttonPaint.setColor(SkColorSetARGB(255, 0, 120, 215));
        SkRect buttonRect = SkRect::MakeXYWH(70 + offset, 200, 100, 30);
        canvas->drawRoundRect(SkRRect::MakeRectXY(buttonRect, 5, 5), buttonPaint);
        
        SkFont buttonFont;
        buttonFont.setSize(14);
        canvas->drawString("Кнопка", 95 + offset, 220, buttonFont, textPaint);
    }
    
    void OnUpdate(float deltaTime) override {
        animationTime_ += deltaTime;
    }
    
    void OnResize(int width, int height) override {
        width_ = width;
        height_ = height;
    }
    
    LayerType GetType() const override { return LayerType::UI; }
    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }
    int GetZOrder() const override { return zOrder_; }
    void SetZOrder(int zOrder) override { zOrder_ = zOrder; }
    
private:
    bool visible_ = true;
    int zOrder_ = 100;
    int width_ = 0;
    int height_ = 0;
    float animationTime_;
};

// Пример оверлей слоя
class OverlayLayer : public ILayer {
public:
    void OnRender(SkCanvas* canvas) override {
        if (!canvas) return;
        
        // FPS счетчик в правом верхнем углу
        SkPaint textPaint;
        textPaint.setColor(SkColorSetARGB(255, 255, 255, 0));
        textPaint.setAntiAlias(true);
        
        SkFont font;
        font.setSize(18);
        
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps_));
        canvas->drawString(fpsText.c_str(), width_ - 100, 30, font, textPaint);
        
        // Информация о слоях
        std::string layerText = "Слоев: " + std::to_string(layerCount_);
        canvas->drawString(layerText.c_str(), width_ - 100, 55, font, textPaint);
        
        // Полупрозрачный индикатор производительности
        SkPaint indicatorPaint;
        if (fps_ > 50) {
            indicatorPaint.setColor(SkColorSetARGB(100, 0, 255, 0)); // Зеленый
        } else if (fps_ > 30) {
            indicatorPaint.setColor(SkColorSetARGB(100, 255, 255, 0)); // Желтый
        } else {
            indicatorPaint.setColor(SkColorSetARGB(100, 255, 0, 0)); // Красный
        }
        
        SkRect indicator = SkRect::MakeXYWH(width_ - 120, 15, 15, 15);
        canvas->drawOval(indicator, indicatorPaint);
    }
    
    void OnUpdate(float deltaTime) override {
        frameCount_++;
        timeAccumulator_ += deltaTime;
        
        // Обновляем FPS каждую секунду
        if (timeAccumulator_ >= 1.0f) {
            fps_ = frameCount_ / timeAccumulator_;
            frameCount_ = 0;
            timeAccumulator_ = 0.0f;
        }
    }
    
    void OnResize(int width, int height) override {
        width_ = width;
        height_ = height;
    }
    
    void SetLayerCount(int count) { layerCount_ = count; }
    
    LayerType GetType() const override { return LayerType::Overlay; }
    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }
    int GetZOrder() const override { return zOrder_; }
    void SetZOrder(int zOrder) override { zOrder_ = zOrder; }
    
private:
    bool visible_ = true;
    int zOrder_ = 1000;
    int width_ = 1280;
    int height_ = 720;
    float fps_ = 60.0f;
    int frameCount_ = 0;
    float timeAccumulator_ = 0.0f;
    int layerCount_ = 0;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Инициализация DPI Awareness
    DPIHelper::SetDPIAwareness(DPIAwareness::PerMonitorV2);
    
    // Создание конфигурации окна
    WindowConfig config;
    config.title = L"Window WinAPI - Демо системы слоев";
    config.width = 1280;
    config.height = 720;
    config.dpiAwareness = DPIAwareness::PerMonitorV2;
    
    // Создание окна
    Window window(config);
    if (!window.Create()) {
        MessageBoxW(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // Инициализация графики
    if (!window.InitializeGraphics(GraphicsAPI::DirectX12) &&
        !window.InitializeGraphics(GraphicsAPI::DirectX11) &&
        !window.InitializeGraphics(GraphicsAPI::Vulkan)) {
        MessageBoxW(nullptr, L"Не удалось инициализировать графический API", L"Ошибка", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // Создание слоев
    auto backgroundLayer = std::make_shared<BackgroundLayer>();
    auto uiLayer = std::make_shared<UILayer>();
    auto overlayLayer = std::make_shared<OverlayLayer>();
    
    // Добавление слоев в систему
    window.GetLayerSystem().AddLayer(backgroundLayer);
    window.GetLayerSystem().AddLayer(uiLayer);
    window.GetLayerSystem().AddLayer(overlayLayer);
    
    overlayLayer->SetLayerCount(3);
    
    // Настройка обработчиков событий
    auto lastTime = std::chrono::steady_clock::now();
    
    window.OnResize = [&](int width, int height) {
        std::wcout << L"Размер окна изменен: " << width << L"x" << height << std::endl;
    };
    
    window.OnKeyboard = [&](UINT key, WPARAM wParam) {
        if (key == VK_SPACE) {
            // Переключение видимости UI слоя по пробелу
            uiLayer->SetVisible(!uiLayer->IsVisible());
        }
        if (key == VK_ESCAPE) {
            window.Close();
        }
    };
    
    window.OnClose = [&]() {
        PostQuitMessage(0);
    };
    
    // Показ окна
    window.Show();
    
    // Основной цикл
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Обновление времени
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Обновление окна
        window.Update(deltaTime);
        
        // Принудительная перерисовка для демонстрации анимации
        InvalidateRect(window.GetHandle(), nullptr, FALSE);
    }
    
    return static_cast<int>(msg.wParam);
}
