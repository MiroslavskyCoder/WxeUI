# Лучшие практики Window WinAPI

Этот документ содержит рекомендации по эффективному использованию библиотеки Window WinAPI.

## Инициализация и настройка

### Выбор графического API

```cpp
// Рекомендованный порядок попыток инициализации
if (!window.InitializeGraphics(GraphicsAPI::DirectX12)) {
    if (!window.InitializeGraphics(GraphicsAPI::DirectX11)) {
        if (!window.InitializeGraphics(GraphicsAPI::Vulkan)) {
            // Fallback на ANGLE
            window.InitializeGraphics(GraphicsAPI::ANGLE);
        }
    }
}
```

**Рекомендации:**
- DirectX12 для новых приложений с HDR поддержкой
- DirectX11 для совместимости с широким спектром устройств
- Vulkan для кросс-платформенных приложений
- ANGLE как последний fallback

### DPI Awareness

```cpp
WindowConfig config;
config.dpiAwareness = DPIAwareness::PerMonitorV2; // Рекомендованное значение
```

**Рекомендации:**
- Всегда используйте `PerMonitorV2` для современных приложений
- Тестируйте на мониторах с разным DPI
- Используйте масштабирование для UI элементов

## Управление производительностью

### Настройка QualityManager

```cpp
// Консервативные настройки для широкого спектра устройств
window.GetQualityManager().SetPerformanceThresholds(45.0f, 60.0f);
window.GetQualityManager().EnableAdaptiveQuality(true);
window.GetQualityManager().SetQualityRange(0.3f, 1.0f);
```

### Оптимизация FrameHigh

```cpp
// Настройка под конкретные потребности
features::FrameHigh::RenderConfig config;
config.targetFPS = 60;  // Начните с 60 FPS
config.enableVSync = true;  // Включите для стабильности
config.adaptiveRefreshRate = true;

// Постепенно увеличивайте FPS при достаточной производительности
if (averageFPS > 55.0f) {
    config.targetFPS = 120;
}
```

### Управление памятью

```cpp
// Настройка под доступную память системы
auto totalRAM = GetTotalSystemMemory();
auto memoryLimit = totalRAM / 4;  // Используем не более 25% системной памяти

window.GetMemoryManager().SetMemoryLimits(memoryLimit);
window.GetMemoryManager().EnableAutoCleanup(true);
window.GetMemoryManager().SetCleanupThreshold(0.8f);  // Очистка при 80% использования
```

## Работа со слоями

### Организация слоев

```cpp
// Правильный порядок Z-order
auto backgroundLayer = std::make_shared<BackgroundLayer>();
backgroundLayer->SetZOrder(-1000);

auto contentLayer = std::make_shared<ContentLayer>();
contentLayer->SetZOrder(0);

auto uiLayer = std::make_shared<UILayer>();
uiLayer->SetZOrder(1000);

auto overlayLayer = std::make_shared<OverlayLayer>();
overlayLayer->SetZOrder(2000);

// Добавление в правильном порядке
window.GetLayerSystem().AddLayer(backgroundLayer);
window.GetLayerSystem().AddLayer(contentLayer);
window.GetLayerSystem().AddLayer(uiLayer);
window.GetLayerSystem().AddLayer(overlayLayer);
```

### Оптимизация рендеринга слоев

```cpp
class OptimizedLayer : public ILayer {
public:
    void OnRender(SkCanvas* canvas) override {
        if (!needsRedraw_) {
            return;  // Пропускаем рендеринг если нет изменений
        }
        
        // Используйте culling для невидимых элементов
        SkRect visibleRect = GetVisibleRect();
        if (!bounds_.intersects(visibleRect)) {
            return;
        }
        
        // Кэшируйте сложные операции
        if (!cachedSurface_ || isDirty_) {
            RenderToCache();
            isDirty_ = false;
        }
        
        canvas->drawImage(cachedSurface_->makeImageSnapshot(), x_, y_);
        needsRedraw_ = false;
    }
    
private:
    bool needsRedraw_ = true;
    bool isDirty_ = true;
    sk_sp<SkSurface> cachedSurface_;
    SkRect bounds_;
};
```

## Event System

### Эффективная обработка событий

```cpp
// Используйте слабые ссылки для предотвращения утечек памяти
class EventHandler {
public:
    EventHandler() {
        events::EventSystem::Subscribe<events::WindowResizeEvent>(
            [weak_self = std::weak_ptr<EventHandler>(shared_from_this())]
            (const events::Event& e) {
                if (auto self = weak_self.lock()) {
                    self->HandleResize(static_cast<const events::WindowResizeEvent&>(e));
                }
            }
        );
    }
    
private:
    void HandleResize(const events::WindowResizeEvent& e) {
        // Обработка изменения размера
    }
};
```

### Приоритизация событий

```cpp
// Установка приоритетов для критичных событий
auto criticalEvent = std::make_unique<CriticalEvent>();
criticalEvent->SetPriority(10);  // Высокий приоритет

events::EventSystem::Dispatch(std::move(criticalEvent));
```

## Мультимониторная поддержка

### Адаптация к разным мониторам

```cpp
void AdaptToMonitor(const features::MultiMonitorSupport::MonitorInfo& monitor) {
    // Настройка под характеристики монитора
    if (monitor.supportHDR) {
        EnableHDRRendering();
    }
    
    if (monitor.refreshRate > 100) {
        window.GetFrameHigh().SetTargetFPS(monitor.refreshRate);
    }
    
    // Адаптация DPI
    float dpiScale = monitor.dpiX / 96.0f;
    SetUIScale(dpiScale);
}
```

## Обработка ошибок

### Graceful degradation

```cpp
class RobustWindow {
public:
    bool Initialize() {
        if (!CreateWindow()) {
            return false;
        }
        
        if (!InitializeGraphics()) {
            // Попытка fallback
            if (!InitializeFallbackGraphics()) {
                return false;
            }
        }
        
        return true;
    }
    
private:
    bool InitializeGraphics() {
        try {
            return window_.InitializeGraphics(PreferredAPI());
        } catch (const std::exception& e) {
            LogError("Graphics initialization failed: " + std::string(e.what()));
            return false;
        }
    }
    
    bool InitializeFallbackGraphics() {
        const GraphicsAPI fallbacks[] = {
            GraphicsAPI::DirectX11,
            GraphicsAPI::ANGLE,
            GraphicsAPI::Software
        };
        
        for (auto api : fallbacks) {
            try {
                if (window_.InitializeGraphics(api)) {
                    LogWarning("Using fallback graphics API");
                    return true;
                }
            } catch (...) {
                continue;
            }
        }
        
        return false;
    }
};
```

## Профилирование и отладка

### Мониторинг производительности

```cpp
class PerformanceTracker {
public:
    void Update() {
        auto stats = window_.GetRenderStats();
        auto perfMetrics = window_.GetFrameHigh().GetPerformanceMetrics();
        
        // Логирование при проблемах с производительностью
        if (stats.fps < targetFPS_ * 0.8f) {
            LogWarning("Low FPS detected: " + std::to_string(stats.fps));
            
            // Автоматическое снижение качества
            window_.GetQualityManager().DecreaseQuality();
        }
        
        // Проверка использования памяти
        auto memoryStats = window_.GetMemoryManager().GetStats();
        if (memoryStats.usagePercent > 0.9f) {
            LogWarning("High memory usage: " + std::to_string(memoryStats.usagePercent * 100) + "%");
            window_.GetMemoryManager().ForceCleanup();
        }
    }
    
private:
    float targetFPS_ = 60.0f;
};
```

## Развертывание

### Конфигурация для разных систем

```cpp
// Автоматическое определение возможностей системы
class SystemProfiler {
public:
    WindowConfig GetOptimalConfig() {
        WindowConfig config;
        
        auto gpuInfo = DetectGPU();
        auto memoryInfo = GetMemoryInfo();
        
        if (gpuInfo.isHighEnd) {
            config.preferredAPI = GraphicsAPI::DirectX12;
            config.enableHDR = true;
            config.targetFPS = 120;
        } else if (gpuInfo.isMidRange) {
            config.preferredAPI = GraphicsAPI::DirectX11;
            config.targetFPS = 60;
        } else {
            config.preferredAPI = GraphicsAPI::ANGLE;
            config.targetFPS = 30;
            config.enableAdaptiveQuality = true;
        }
        
        return config;
    }
};
```

## Заключение

Следование этим рекомендациям поможет создать производительные и стабильные приложения с использованием Window WinAPI. Всегда тестируйте на различных конфигурациях оборудования и мониторьте производительность в runtime.
