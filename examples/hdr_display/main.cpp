#include "../../include/window_winapi.h"
#include <iostream>

using namespace WxeUI;

class HDRLayer : public ILayer {
public:
    HDRLayer() : visible_(true), zOrder_(0) {}
    
    void OnRender(SkCanvas* canvas) override {
        if (!visible_) return;
        
        // HDR демонстрация с расширенным цветовым пространством
        SkPaint paint;
        
        // Использование P3 цветового пространства для Wide Color Gamut
        auto colorSpace = SkColorSpace::MakeRGB(
            SkNamedTransferFn::kRec2020,
            SkNamedGamut::kRec2020
        );
        
        // Рисование с высокой яркостью (HDR)
        paint.setColor4f({1.2f, 0.8f, 0.2f, 1.0f}, colorSpace.get());
        paint.setAntiAlias(true);
        
        SkRect hdrRect = SkRect::MakeXYWH(100, 100, 300, 200);
        canvas->drawRect(hdrRect, paint);
        
        // Текст с HDR поддержкой
        paint.setColor4f({0.9f, 0.9f, 1.5f, 1.0f}, colorSpace.get());
        SkFont font;
        font.setSize(32);
        canvas->drawString("HDR Display Demo", 120, 220, font, paint);
    }
    
    void OnUpdate(float deltaTime) override {}
    void OnResize(int width, int height) override {}
    
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
    WindowConfig config;
    config.title = L"HDR Display Demo";
    config.width = 1280;
    config.height = 720;
    
    Window window(config);
    
    if (!window.Create() || !window.InitializeGraphics(GraphicsAPI::DirectX12)) {
        MessageBoxW(nullptr, L"Не удалось создать окно!", L"Ошибка", MB_ICONERROR);
        return -1;
    }
    
    auto hdrLayer = std::make_shared<HDRLayer>();
    window.GetLayerSystem().AddLayer(hdrLayer);
    
    window.Show();
    
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        window.Render();
    }
    
    return static_cast<int>(msg.wParam);
}
