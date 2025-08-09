#include "window_winapi.h"
#include <dwmapi.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")

namespace WxeUI {

// Статические переменные класса Window
const wchar_t* Window::ClassName = L"WindowWinAPIClass";
bool Window::classRegistered_ = false;

// Конструктор Window
Window::Window(const WindowConfig& config) 
    : hwnd_(nullptr)
    , config_(config)
    , width_(config.width)
    , height_(config.height)
    , dpiScale_(1.0f)
    , isVisible_(false)
    , frameHigh_(this) {
    
    // Установка DPI Awareness
    DPIHelper::SetDPIAwareness(config_.dpiAwareness);
    
    // Инициализация времени последнего кадра
    lastFrameTime_ = std::chrono::steady_clock::now();
    
    // Инициализация систем
    memoryManager_.Initialize();
    qualityManager_.Initialize();
    performanceMonitor_.Initialize();
    
    // Включение event system по умолчанию
    EnableEventSystem(true);
}

// Деструктор Window
Window::~Window() {
    Destroy();
}

// Создание окна
bool Window::Create() {
    // Регистрация класса окна
    if (!classRegistered_) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = StaticWindowProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = GetModuleHandleW(nullptr);
        wcex.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr; // Не используем фон, так как рисуем сами
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = ClassName;
        wcex.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
        
        if (!RegisterClassExW(&wcex)) {
            return false;
        }
        classRegistered_ = true;
    }
    
    // Вычисление размеров окна с учетом DPI
    RECT rect = { 0, 0, config_.width, config_.height };
    DPIHelper::AdjustWindowRectForDPI(&rect, config_.style, config_.exStyle, nullptr);
    
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    
    // Создание окна
    hwnd_ = CreateWindowExW(
        config_.exStyle,
        ClassName,
        config_.title.c_str(),
        config_.style,
        config_.x, config_.y,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this
    );
    
    if (!hwnd_) {
        return false;
    }
    
    // Обновление DPI
    UpdateDPI();
    
    // Включение композиции DWM
    BOOL enable = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(enable));
    
    return true;
}

// Уничтожение окна
void Window::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// Инициализация графики
bool Window::InitializeGraphics(GraphicsAPI api) {
    if (!hwnd_) {
        return false;
    }
    
    switch (api) {
        case GraphicsAPI::DirectX12:
            graphicsContext_ = std::make_unique<DirectX12Context>();
            break;
        case GraphicsAPI::DirectX11:
            graphicsContext_ = std::make_unique<DirectX11Context>();
            break;
        case GraphicsAPI::Vulkan:
            graphicsContext_ = std::make_unique<VulkanContext>();
            break;
        default:
            return false;
    }
    
    return graphicsContext_->Initialize(hwnd_, width_, height_);
}

// Показ окна
void Window::Show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        isVisible_ = true;
    }
}

// Скрытие окна
void Window::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        isVisible_ = false;
    }
}

// Статическая оконная процедура
LRESULT CALLBACK Window::StaticWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        window = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (window) {
        return window->WindowProc(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Оконная процедура
LRESULT Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            int newWidth = LOWORD(lParam);
            int newHeight = HIWORD(lParam);
            
            if (newWidth > 0 && newHeight > 0 && (newWidth != width_ || newHeight != height_)) {
                width_ = newWidth;
                height_ = newHeight;
                
                if (graphicsContext_) {
                    graphicsContext_->ResizeBuffers(width_, height_);
                }
                
                layerSystem_.ResizeLayers(width_, height_);
                
                // Уведомление через event system
                if (eventSystemEnabled_) {
                    events::EventSystem::Dispatch(
                        std::make_unique<events::WindowResizeEvent>(width_, height_)
                    );
                }
                
                if (OnResize) {
                    OnResize(width_, height_);
                }
            }
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            Render();
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_DPICHANGED: {
            RECT* newRect = reinterpret_cast<RECT*>(lParam);
            float oldDPI = dpiScale_;
            
            UpdateDPI();
            
            // Обновление размеров окна под новый DPI
            SetWindowPos(hwnd_, nullptr, 
                newRect->left, newRect->top,
                newRect->right - newRect->left,
                newRect->bottom - newRect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            
            // Уведомление через event system
            if (eventSystemEnabled_) {
                events::EventSystem::Dispatch(
                    std::make_unique<events::DPIChangedEvent>(oldDPI, dpiScale_)
                );
            }
            
            if (OnDPIChanged) {
                OnDPIChanged(dpiScale_);
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            if (eventSystemEnabled_) {
                events::EventSystem::Dispatch(
                    std::make_unique<events::MouseMoveEvent>(x, y)
                );
            }
            
            if (OnMouseMove) {
                OnMouseMove(x, y, static_cast<UINT>(wParam));
            }
            return 0;
        }
        
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            int button = 0;
            bool pressed = false;
            
            switch (msg) {
                case WM_LBUTTONDOWN: button = 0; pressed = true; break;
                case WM_RBUTTONDOWN: button = 1; pressed = true; break;
                case WM_MBUTTONDOWN: button = 2; pressed = true; break;
                case WM_LBUTTONUP: button = 0; pressed = false; break;
                case WM_RBUTTONUP: button = 1; pressed = false; break;
                case WM_MBUTTONUP: button = 2; pressed = false; break;
            }
            
            if (eventSystemEnabled_) {
                events::EventSystem::Dispatch(
                    std::make_unique<events::MouseButtonEvent>(button, pressed)
                );
            }
            
            if (OnMouseButton) {
                OnMouseButton(button, static_cast<UINT>(wParam));
            }
            return 0;
        }
        
        case WM_KEYDOWN:
        case WM_KEYUP: {
            bool pressed = (msg == WM_KEYDOWN);
            bool repeat = (lParam & 0x40000000) != 0;
            
            if (eventSystemEnabled_) {
                events::EventSystem::Dispatch(
                    std::make_unique<events::KeyboardEvent>(static_cast<int>(wParam), pressed, repeat)
                );
            }
            
            if (OnKeyboard) {
                OnKeyboard(msg, wParam);
            }
            return 0;
        }
        
        case WM_CLOSE:
            if (eventSystemEnabled_) {
                events::EventSystem::Dispatch(
                    std::make_unique<events::WindowCloseEvent>()
                );
            }
            
            if (OnClose) {
                OnClose();
            } else {
                PostQuitMessage(0);
            }
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Получение информации о дисплее
DisplayInfo Window::GetDisplayInfo() const {
    return DPIHelper::GetDisplayInfo(hwnd_);
}

// Обновление DPI
void Window::UpdateDPI() {
    if (hwnd_) {
        dpiScale_ = DPIHelper::GetDPIScale(hwnd_);
    }
}

// Рендеринг
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
    
    // Рендеринг слоев
    layerSystem_.RenderLayers(canvas);
    
    // Пользовательский рендеринг
    if (OnRender) {
        OnRender(canvas);
    }
    
    // Презентация
    graphicsContext_->Present();
    
    // Обновление статистики
    UpdateRenderStats();
}

// Обновление статистики рендеринга
void Window::UpdateRenderStats() {
    auto currentTime = std::chrono::steady_clock::now();
    auto frameTime = currentTime - lastFrameTime_;
    lastFrameTime_ = currentTime;
    
    renderStats_.frameTime = std::chrono::duration_cast<std::chrono::nanoseconds>(frameTime);
    renderStats_.frameCount++;
    
    // Вычисление FPS (среднее за последние 60 кадров)
    static std::vector<std::chrono::nanoseconds> frameTimes;
    frameTimes.push_back(renderStats_.frameTime);
    if (frameTimes.size() > 60) {
        frameTimes.erase(frameTimes.begin());
    }
    
    if (!frameTimes.empty()) {
        auto avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), std::chrono::nanoseconds(0)) / frameTimes.size();
        renderStats_.fps = 1000000000.0f / static_cast<float>(avgFrameTime.count());
    }
}

} // namespace window_winapi
