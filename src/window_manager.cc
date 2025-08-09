#include "window_winapi.h"
#include <unordered_map>
#include <algorithm>

namespace WxeUI {

// Простой менеджер окон для управления множественными окнами
class WindowManager {
public:
    static WindowManager& Instance() {
        static WindowManager instance;
        return instance;
    }
    
    void RegisterWindow(HWND hwnd, Window* window) {
        windows_[hwnd] = window;
    }
    
    void UnregisterWindow(HWND hwnd) {
        windows_.erase(hwnd);
    }
    
    Window* GetWindow(HWND hwnd) {
        auto it = windows_.find(hwnd);
        return (it != windows_.end()) ? it->second : nullptr;
    }
    
    std::vector<Window*> GetAllWindows() {
        std::vector<Window*> result;
        for (const auto& pair : windows_) {
            if (pair.second) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    size_t GetWindowCount() const {
        return windows_.size();
    }
    
private:
    std::unordered_map<HWND, Window*> windows_;
};

// Дополнительные методы для Window класса
void Window::SetGraphicsContext(std::unique_ptr<IGraphicsContext> context) {
    graphicsContext_ = std::move(context);
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
    
    // Пользовательское обновление
    if (OnUpdate) {
        OnUpdate(deltaTime);
    }
}

void Window::OpenScreen(const std::string& screenName) {
    // TODO: Реализация системы экранов
    // Заглушка для демонстрации API
    std::wcout << L"Открытие экрана: " << utils::StringToWString(screenName).c_str() << std::endl;
}

void Window::FrameHigh() {
    // TODO: Реализация высокочастотного рендеринга
    // Заглушка для демонстрации API
    Render();
}

sk_sp<SkSurface> Window::ToFrame(int width, int height) {
    // Создание поверхности для рендеринга в текстуру
    SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
    
    if (surface) {
        SkCanvas* canvas = surface->getCanvas();
        if (canvas) {
            // Рендеринг слоев на поверхность
            layerSystem_.RenderLayers(canvas);
            
            // Пользовательский рендеринг
            if (OnRender) {
                OnRender(canvas);
            }
        }
    }
    
    return surface;
}

} // namespace window_winapi
