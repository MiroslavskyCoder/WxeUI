#include "../../include/window_winapi.h"
#include <iostream>

using namespace WxeUI;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    features::MultiMonitorSupport multiMonitor;
    
    auto monitors = multiMonitor.GetMonitors();
    std::wcout << L"Обнаружено мониторов: " << monitors.size() << std::endl;
    
    for (const auto& monitor : monitors) {
        std::wcout << L"Монитор: " << monitor.name.c_str() 
                  << L", Разрешение: " << (monitor.bounds.right - monitor.bounds.left)
                  << L"x" << (monitor.bounds.bottom - monitor.bounds.top)
                  << L", DPI: " << monitor.dpiX << L"x" << monitor.dpiY
                  << L", Частота: " << monitor.refreshRate << L"Hz"
                  << L", HDR: " << (monitor.supportHDR ? L"Да" : L"Нет")
                  << std::endl;
    }
    
    // Создание окна на каждом мониторе
    std::vector<std::unique_ptr<Window>> windows;
    
    for (size_t i = 0; i < monitors.size() && i < 4; ++i) {
        const auto& monitor = monitors[i];
        
        WindowConfig config;
        config.title = L"Multi-Monitor Demo " + std::to_wstring(i + 1);
        config.width = 800;
        config.height = 600;
        
        auto window = std::make_unique<Window>(config);
        
        if (window->Create() && window->InitializeGraphics(GraphicsAPI::DirectX11)) {
            // Перемещение окна на соответствующий монитор
            multiMonitor.MoveWindowToMonitor(window->GetHandle(), monitor);
            
            window->OnRender = [i](SkCanvas* canvas) {
                SkPaint paint;
                paint.setColor(SK_ColorBLUE);
                paint.setAntiAlias(true);
                
                SkRect rect = SkRect::MakeXYWH(50, 50, 200, 100);
                canvas->drawRect(rect, paint);
                
                paint.setColor(SK_ColorWHITE);
                SkFont font;
                font.setSize(24);
                
                std::string text = "Монитор " + std::to_string(i + 1);
                canvas->drawString(text.c_str(), 60, 110, font, paint);
            };
            
            window->Show();
            windows.push_back(std::move(window));
        }
    }
    
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        for (auto& window : windows) {
            if (window && window->IsValid()) {
                window->Render();
            }
        }
    }
    
    return static_cast<int>(msg.wParam);
}
