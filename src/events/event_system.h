#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <typeindex>
#include <any>

namespace WxeUI {
namespace events {

// Базовый класс для всех событий
class Event {
public:
    virtual ~Event() = default;
    virtual std::type_index GetType() const = 0;
    virtual std::string GetName() const = 0;
    
    bool IsHandled() const { return handled_; }
    void SetHandled(bool handled = true) { handled_ = handled; }
    
    int GetPriority() const { return priority_; }
    void SetPriority(int priority) { priority_ = priority; }
    
private:
    bool handled_ = false;
    int priority_ = 0;
};

// Макрос для создания событий
#define DEFINE_EVENT(EventClass) \
    std::type_index GetType() const override { return std::type_index(typeid(EventClass)); } \
    std::string GetName() const override { return #EventClass; }

// Конкретные события
class WindowResizeEvent : public Event {
public:
    DEFINE_EVENT(WindowResizeEvent)
    
    WindowResizeEvent(int width, int height) : width_(width), height_(height) {}
    
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    
private:
    int width_, height_;
};

class WindowCloseEvent : public Event {
public:
    DEFINE_EVENT(WindowCloseEvent)
};

class MouseMoveEvent : public Event {
public:
    DEFINE_EVENT(MouseMoveEvent)
    
    MouseMoveEvent(int x, int y) : x_(x), y_(y) {}
    
    int GetX() const { return x_; }
    int GetY() const { return y_; }
    
private:
    int x_, y_;
};

class MouseButtonEvent : public Event {
public:
    DEFINE_EVENT(MouseButtonEvent)
    
    MouseButtonEvent(int button, bool pressed) : button_(button), pressed_(pressed) {}
    
    int GetButton() const { return button_; }
    bool IsPressed() const { return pressed_; }
    
private:
    int button_;
    bool pressed_;
};

class KeyboardEvent : public Event {
public:
    DEFINE_EVENT(KeyboardEvent)
    
    KeyboardEvent(int key, bool pressed, bool repeat = false) 
        : key_(key), pressed_(pressed), repeat_(repeat) {}
    
    int GetKey() const { return key_; }
    bool IsPressed() const { return pressed_; }
    bool IsRepeat() const { return repeat_; }
    
private:
    int key_;
    bool pressed_;
    bool repeat_;
};

class DPIChangedEvent : public Event {
public:
    DEFINE_EVENT(DPIChangedEvent)
    
    DPIChangedEvent(float oldDPI, float newDPI) : oldDPI_(oldDPI), newDPI_(newDPI) {}
    
    float GetOldDPI() const { return oldDPI_; }
    float GetNewDPI() const { return newDPI_; }
    
private:
    float oldDPI_, newDPI_;
};

class RenderEvent : public Event {
public:
    DEFINE_EVENT(RenderEvent)
    
    RenderEvent(class SkCanvas* canvas) : canvas_(canvas) {}
    
    class SkCanvas* GetCanvas() const { return canvas_; }
    
private:
    class SkCanvas* canvas_;
};

class UpdateEvent : public Event {
public:
    DEFINE_EVENT(UpdateEvent)
    
    UpdateEvent(float deltaTime) : deltaTime_(deltaTime) {}
    
    float GetDeltaTime() const { return deltaTime_; }
    
private:
    float deltaTime_;
};

// Event listener
using EventListener = std::function<void(const Event&)>;

// Event dispatcher
class EventDispatcher {
public:
    EventDispatcher();
    ~EventDispatcher();
    
    // Подписка на события
    template<typename T>
    void Subscribe(EventListener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_[std::type_index(typeid(T))].push_back(listener);
    }
    
    // Отписка от событий
    template<typename T>
    void Unsubscribe() {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_[std::type_index(typeid(T))].clear();
    }
    
    // Отправка событий
    void Dispatch(std::unique_ptr<Event> event);
    void DispatchImmediate(std::unique_ptr<Event> event);
    
    // Управление потоками
    void StartProcessing();
    void StopProcessing();
    bool IsProcessing() const { return processing_; }
    
    // Настройки
    void SetMaxQueueSize(size_t maxSize) { maxQueueSize_ = maxSize; }
    void SetProcessingThreadCount(int count);
    
    // Статистика
    size_t GetQueueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return eventQueue_.size();
    }
    
private:
    struct QueuedEvent {
        std::unique_ptr<Event> event;
        std::chrono::steady_clock::time_point timestamp;
        
        bool operator<(const QueuedEvent& other) const {
            return event->GetPriority() < other.event->GetPriority();
        }
    };
    
    std::unordered_map<std::type_index, std::vector<EventListener>> listeners_;
    std::mutex mutex_;
    
    std::priority_queue<QueuedEvent> eventQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    std::vector<std::unique_ptr<std::thread>> processingThreads_;
    std::atomic<bool> processing_{false};
    std::atomic<bool> shouldStop_{false};
    
    size_t maxQueueSize_ = 1000;
    
    void ProcessEvents();
    void DispatchToListeners(const Event& event);
};

// Глобальный event dispatcher
class EventSystem {
public:
    static EventDispatcher& GetDispatcher() {
        static EventDispatcher dispatcher;
        return dispatcher;
    }
    
    template<typename T>
    static void Subscribe(EventListener listener) {
        GetDispatcher().Subscribe<T>(listener);
    }
    
    template<typename T>
    static void Unsubscribe() {
        GetDispatcher().Unsubscribe<T>();
    }
    
    static void Dispatch(std::unique_ptr<Event> event) {
        GetDispatcher().Dispatch(std::move(event));
    }
    
    static void DispatchImmediate(std::unique_ptr<Event> event) {
        GetDispatcher().DispatchImmediate(std::move(event));
    }
};

} // namespace events
} // namespace window_winapi
