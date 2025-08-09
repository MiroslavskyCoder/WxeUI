#include "src/window_winapi.h"
#include <iostream>
#include <vector>
#include <random>

using namespace WxeUI;

class BenchmarkLayer : public ILayer {
public:
    BenchmarkLayer() : visible_(true), zOrder_(0) {
        // Генерация случайных прямоугольников для бенчмарка
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> posDist(0, 1000);
        std::uniform_int_distribution<> sizeDist(10, 100);
        std::uniform_int_distribution<> colorDist(0, 255);
        
        for (int i = 0; i < 1000; ++i) {
            TestRect rect;
            rect.x = posDist(gen);
            rect.y = posDist(gen);
            rect.width = sizeDist(gen);
            rect.height = sizeDist(gen);
            rect.r = colorDist(gen);
            rect.g = colorDist(gen);
            rect.b = colorDist(gen);
            testRects_.push_back(rect);
        }
    }
    
    void OnRender(SkCanvas* canvas) override {
        if (!visible_) return;
        
        // Бенчмарк: рисование множества прямоугольников
        for (const auto& rect : testRects_) {
            SkPaint paint;
            paint.setColor(SkColorSetRGB(rect.r, rect.g, rect.b));
            paint.setAntiAlias(true);
            
            SkRect skRect = SkRect::MakeXYWH(
                static_cast<float>(rect.x), 
                static_cast<float>(rect.y), 
                static_cast<float>(rect.width), 
                static_cast<float>(rect.height)
            );
            
            canvas->drawRect(skRect, paint);
        }
        
        // Информация о производительности
        frameCount_++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime_).count();
        
        if (elapsed > 0) {
            float avgFPS = static_cast<float>(frameCount_) / elapsed;
            
            SkPaint textPaint;
            textPaint.setColor(SK_ColorYELLOW);
            textPaint.setAntiAlias(true);
            
            SkFont font;
            font.setSize(32);
            
            std::string fpsText = "Benchmark FPS: " + std::to_string(static_cast<int>(avgFPS));
            canvas->drawString(fpsText.c_str(), 50, 50, font, textPaint);
            
            std::string objectsText = "Объектов: " + std::to_string(testRects_.size());
            canvas->drawString(objectsText.c_str(), 50, 90, font, textPaint);
        }
    }
    
    void OnUpdate(float deltaTime) override {
        // Анимация прямоугольников для более реалистичного бенчмарка
        for (auto& rect : testRects_) {
            rect.x += static_cast<int>(rect.speedX * deltaTime * 60.0f);
            rect.y += static_cast<int>(rect.speedY * deltaTime * 60.0f);
            
            if (rect.x < 0 || rect.x > 1200) rect.speedX = -rect.speedX;
            if (rect.y < 0 || rect.y > 700) rect.speedY = -rect.speedY;
        }
    }
    
    void OnResize(int width, int height) override {}
    
    LayerType GetType() const override { return LayerType::Content; }
    bool IsVisible() const override { return visible_; }
    void SetVisible(bool visible) override { visible_ = visible; }
    int GetZOrder() const override { return zOrder_; }
    void SetZOrder(int zOrder) override { zOrder_ = zOrder; }
    
private:
    struct TestRect {
        int x, y, width, height;
        int r, g, b;
        float speedX = 1.0f, speedY = 1.0f;
    };
    
    std::vector<TestRect> testRects_;
    bool visible_;
    int zOrder_;
    int frameCount_ = 0;
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WindowConfig config;
    config.title = L"Performance Benchmark";
    config.width = 1280;
    config.height = 720;
    
    Window window(config);
    
    if (!window.Create() || !window.InitializeGraphics(GraphicsAPI::DirectX12)) {
        if (!window.InitializeGraphics(GraphicsAPI::DirectX11)) {
            MessageBoxW(nullptr, L"Не удалось инициализировать графику!", L"Ошибка", MB_ICONERROR);
            return -1;
        }
    }
    
    auto benchmarkLayer = std::make_shared<BenchmarkLayer>();
    window.GetLayerSystem().AddLayer(benchmarkLayer);
    
    // Включение высокочастотного рендеринга для бенчмарка
    window.FrameHigh();
    
    window.Show();
    
    MSG msg = {};
    auto lastTime = std::chrono::steady_clock::now();
    
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        window.Update(deltaTime);
        
        // Статистика производительности
        static auto lastStatsTime = std::chrono::steady_clock::now();
        if (currentTime - lastStatsTime > std::chrono::seconds(1)) {
            auto stats = window.GetRenderStats();
            auto perfStats = window.GetFrameHigh().GetPerformanceMetrics();
            
            std::wcout << L"=== Статистика производительности ===" << std::endl;
            std::wcout << L"FPS: " << stats.fps << std::endl;
            std::wcout << L"Время кадра: " << std::chrono::duration<float, std::milli>(stats.frameTime).count() << L"ms" << std::endl;
            std::wcout << L"FrameHigh FPS: " << perfStats.currentFPS << std::endl;
            std::wcout << L"Пропущенные кадры: " << perfStats.droppedFrames << std::endl;
            std::wcout << L"Jitter: " << perfStats.jitter << std::endl;
            
            lastStatsTime = currentTime;
        }
    }
    
    return static_cast<int>(msg.wParam);
}
