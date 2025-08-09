#include "../../include/window_winapi.h"
#include <iostream>

using namespace WxeUI;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Инициализация DPI Awareness
    DPIHelper::SetDPIAwareness(DPIAwareness::PerMonitorV2);
    
    // Создание конфигурации окна
    WindowConfig config;
    config.title = L"Window WinAPI - Базовый пример";
    config.width = 1280;
    config.height = 720;
    config.dpiAwareness = DPIAwareness::PerMonitorV2;
    
    // Создание окна
    Window window(config);
    if (!window.Create()) {
        MessageBoxW(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // Попытка инициализации DirectX 12, затем DirectX 11, затем Vulkan
    GraphicsAPI selectedAPI = GraphicsAPI::Software;
    
    if (window.InitializeGraphics(GraphicsAPI::DirectX12)) {
        selectedAPI = GraphicsAPI::DirectX12;
        std::wcout << L"Используется DirectX 12" << std::endl;
    } else if (window.InitializeGraphics(GraphicsAPI::DirectX11)) {
        selectedAPI = GraphicsAPI::DirectX11;
        std::wcout << L"Используется DirectX 11" << std::endl;
    } else if (window.InitializeGraphics(GraphicsAPI::Vulkan)) {
        selectedAPI = GraphicsAPI::Vulkan;
        std::wcout << L"Используется Vulkan" << std::endl;
    } else {
        MessageBoxW(nullptr, L"Не удалось инициализировать графический API", L"Ошибка", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // Настройка обработчиков событий
    window.OnRender = [&](SkCanvas* canvas) {
        // Очистка фона
        canvas->clear(SkColorSetARGB(255, 45, 45, 48));
        
        // Получение размеров окна
        int width = window.GetWidth();
        int height = window.GetHeight();
        float dpiScale = window.GetDPIScale();
        
        // Настройка кисти для текста
        SkPaint textPaint;
        textPaint.setColor(SK_ColorWHITE);
        textPaint.setAntiAlias(true);
        
        SkFont font;
        font.setSize(24 * dpiScale);
        
        // Отображение информации о API
        std::string apiText = "Графический API: ";
        switch (selectedAPI) {
            case GraphicsAPI::DirectX12: apiText += "DirectX 12"; break;
            case GraphicsAPI::DirectX11: apiText += "DirectX 11"; break;
            case GraphicsAPI::Vulkan: apiText += "Vulkan"; break;
            case GraphicsAPI::ANGLE: apiText += "ANGLE"; break;
            default: apiText += "Software"; break;
        }
        
        canvas->drawString(apiText.c_str(), 50 * dpiScale, 80 * dpiScale, font, textPaint);
        
        // Отображение информации о DPI
        std::string dpiText = "DPI масштаб: " + std::to_string(dpiScale);
        canvas->drawString(dpiText.c_str(), 50 * dpiScale, 120 * dpiScale, font, textPaint);
        
        // Отображение размеров окна
        std::string sizeText = "Размер: " + std::to_string(width) + "x" + std::to_string(height);
        canvas->drawString(sizeText.c_str(), 50 * dpiScale, 160 * dpiScale, font, textPaint);
        
        // Рисование прямоугольника с градиентом
        SkPaint rectPaint;
        SkPoint points[2] = {
            SkPoint::Make(100 * dpiScale, 200 * dpiScale),
            SkPoint::Make(400 * dpiScale, 300 * dpiScale)
        };
        SkColor colors[2] = { SK_ColorBLUE, SK_ColorCYAN };
        rectPaint.setShader(SkGradientShader::MakeLinear(points, colors, nullptr, 2, SkTileMode::kClamp));
        
        SkRect rect = SkRect::MakeXYWH(100 * dpiScale, 200 * dpiScale, 300 * dpiScale, 100 * dpiScale);
        canvas->drawRoundRect(SkRRect::MakeRectXY(rect, 10 * dpiScale, 10 * dpiScale), rectPaint);
        
        // Статистика рендеринга
        RenderStats stats = window.GetRenderStats();
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(stats.fps));
        canvas->drawString(fpsText.c_str(), width - 150 * dpiScale, 50 * dpiScale, font, textPaint);
    };
    
    window.OnResize = [&](int width, int height) {
        std::wcout << L"Размер окна изменен: " << width << L"x" << height << std::endl;
    };
    
    window.OnDPIChanged = [&](float newScale) {
        std::wcout << L"DPI изменен: " << newScale << std::endl;
    };
    
    window.OnClose = [&]() {
        PostQuitMessage(0);
    };
    
    // Показ окна
    window.Show();
    
    // Основной цикл сообщений
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return static_cast<int>(msg.wParam);
}
