# Продвинутые возможности Window WinAPI

Эта документация описывает продвинутые возможности библиотеки Window WinAPI, включая OpenScreen, FrameHigh, мультимониторную поддержку и систему событий.

## OpenScreen - Совместное использование экранов

### Описание
OpenScreen позволяет создавать и управлять совместно используемыми экранами между несколькими клиентами. Поддерживает HDR, Wide Color Gamut и адаптивное качество.

### Основные возможности
- Создание именованных экранов
- Управление подключенными клиентами
- Адаптивное качество в зависимости от производительности
- Поддержка HDR и расширенного цветового пространства
- Автоматическая очистка неактивных соединений

### Пример использования
```cpp
#include "features/openscreen.h"

// Создание экрана
features::OpenScreen::ScreenConfig config;
config.name = "main_screen";
config.width = 1920;
config.height = 1080;
config.enableHDR = true;
config.maxClients = 4;

window.GetOpenScreen().CreateScreen("main_screen", config);

// Подключение клиента
window.GetOpenScreen().ShareScreen("main_screen", "client_1");

// Настройка обработчиков событий
window.GetOpenScreen().OnClientConnected = [](const std::string& screen, const auto& client) {
    std::cout << "Клиент подключен: " << client.id << std::endl;
};
```

## FrameHigh - Высокочастотный рендеринг

### Описание
FrameHigh обеспечивает рендеринг с высокой частотой кадров (120+ FPS) с автоматической адаптацией качества.

### Возможности
- Целевые FPS до 240
- Адаптивный refresh rate
- Поддержка VSync, FreeSync, G-Sync
- Автоматическая настройка качества
- Мониторинг производительности в реальном времени

### Пример использования
```cpp
// Настройка высокочастотного рендеринга
features::FrameHigh::RenderConfig config;
config.targetFPS = 144;
config.enableFreeSync = true;
config.adaptiveRefreshRate = true;

window.GetFrameHigh().SetRenderConfig(config);
window.GetFrameHigh().StartHighFrequencyRendering();

// Мониторинг производительности
window.GetFrameHigh().OnPerformanceUpdate = [](const auto& metrics) {
    std::cout << "FPS: " << metrics.currentFPS 
              << ", Jitter: " << metrics.jitter << std::endl;
};
```

## MultiMonitorSupport - Мультимониторная поддержка

### Описание
Поддержка нескольких мониторов с разными DPI, частотами обновления и возможностями HDR.

### Возможности
- Автоматическое обнаружение всех мониторов
- Получение информации о DPI и частоте обновления
- Определение поддержки HDR и Wide Color Gamut
- Перемещение окон между мониторами
- Максимизация на конкретном мониторе

### Пример использования
```cpp
// Получение информации о мониторах
auto multiMonitor = window.GetMultiMonitorSupport();
auto monitors = multiMonitor.GetMonitors();

for (const auto& monitor : monitors) {
    std::cout << "Монитор: " << monitor.name
              << ", Разрешение: " << (monitor.bounds.right - monitor.bounds.left)
              << "x" << (monitor.bounds.bottom - monitor.bounds.top)
              << ", DPI: " << monitor.dpiX
              << ", HDR: " << (monitor.supportHDR ? "Да" : "Нет")
              << std::endl;
}

// Перемещение окна на второй монитор
if (monitors.size() > 1) {
    multiMonitor.MoveWindowToMonitor(window.GetHandle(), monitors[1]);
}
```

## Event System - Система событий

### Описание
Современная event-driven архитектура с поддержкой мультипоточности, приоритетов и фильтрации.

### Возможности
- Типобезопасные события
- Мультипоточная обработка
- Система приоритетов
- Автоматическая очистка
- Кастомные события

### Встроенные события
- `WindowResizeEvent` - изменение размера окна
- `WindowCloseEvent` - закрытие окна
- `MouseMoveEvent` - движение мыши
- `MouseButtonEvent` - нажатие кнопок мыши
- `KeyboardEvent` - клавиатурные события
- `DPIChangedEvent` - изменение DPI
- `RenderEvent` - событие рендеринга
- `UpdateEvent` - событие обновления

### Пример использования
```cpp
// Подписка на события
events::EventSystem::Subscribe<events::WindowResizeEvent>([](const events::Event& e) {
    const auto& resizeEvent = static_cast<const events::WindowResizeEvent&>(e);
    std::cout << "Размер изменен на " << resizeEvent.GetWidth() 
              << "x" << resizeEvent.GetHeight() << std::endl;
});

// Создание кастомного события
class CustomEvent : public events::Event {
public:
    DEFINE_EVENT(CustomEvent)
    CustomEvent(int value) : value_(value) {}
    int GetValue() const { return value_; }
private:
    int value_;
};

// Отправка события
events::EventSystem::Dispatch(std::make_unique<CustomEvent>(42));
```

## Система управления производительностью

### QualityManager
Автоматическое управление качеством рендеринга для поддержания стабильного FPS.

```cpp
// Настройка адаптивного качества
window.GetQualityManager().EnableAdaptiveQuality(true);
window.GetQualityManager().SetPerformanceThresholds(30.0f, 60.0f);
```

### MemoryManager
Оптимизация использования памяти с автоматической очисткой.

```cpp
// Настройка лимитов памяти
window.GetMemoryManager().SetMemoryLimits(512 * 1024 * 1024); // 512MB
window.GetMemoryManager().EnableAutoCleanup(true);
```

### PerformanceMonitor
Мониторинг производительности в реальном времени.

```cpp
// Получение метрик производительности
auto metrics = window.GetPerformanceMonitor().GetCurrentMetrics();
std::cout << "CPU времени: " << metrics.cpuTime << "ms\n";
std::cout << "GPU времени: " << metrics.gpuTime << "ms\n";
```

## Интеграция систем

Все системы полностью интегрированы и работают совместно:

1. **QualityManager** автоматически снижает качество при падении FPS в **FrameHigh**
2. **MemoryManager** очищает кэши при превышении лимитов
3. **Event System** уведомляет о всех изменениях состояния
4. **OpenScreen** использует общие системы качества и памяти
5. **MultiMonitorSupport** учитывает особенности каждого дисплея

## Рекомендации по производительности

1. Используйте **DirectX12** для лучшей производительности и HDR
2. Включайте **FrameHigh** только при необходимости высокого FPS
3. Настраивайте лимиты памяти в зависимости от системы
4. Используйте адаптивное качество для стабильной производительности
5. Мониторьте метрики через **PerformanceMonitor**

## Примеры

Полные примеры использования всех возможностей доступны в папке `examples/`:
- `complete_showcase/` - демонстрация всех возможностей
- `hdr_display/` - работа с HDR и Wide Color
- `multi_monitor/` - мультимониторная поддержка
- `performance_benchmark/` - комплексные бенчмарки
