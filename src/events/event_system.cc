#include "events/event_system.h"
#include <algorithm>
#include <iostream>

namespace WxeUI {
namespace events {

// EventDispatcher Implementation
EventDispatcher::EventDispatcher() {
    StartProcessing();
}

EventDispatcher::~EventDispatcher() {
    StopProcessing();
}

void EventDispatcher::Dispatch(std::unique_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    QueuedEvent queuedEvent;
    queuedEvent.event = std::move(event);
    queuedEvent.timestamp = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        // Проверка лимита очереди
        if (eventQueue_.size() >= maxQueueSize_) {
            // Удаление самого старого события с низким приоритетом
            auto temp = std::priority_queue<QueuedEvent>();
            while (!eventQueue_.empty() && temp.size() < maxQueueSize_ - 1) {
                temp.push(eventQueue_.top());
                eventQueue_.pop();
            }
            eventQueue_ = std::move(temp);
        }
        
        eventQueue_.push(std::move(queuedEvent));
    }
    
    queueCondition_.notify_one();
}

void EventDispatcher::DispatchImmediate(std::unique_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    DispatchToListeners(*event);
}

void EventDispatcher::StartProcessing() {
    if (processing_) {
        return;
    }
    
    processing_ = true;
    shouldStop_ = false;
    
    // Запуск потоков обработки
    int threadCount = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    for (int i = 0; i < threadCount; ++i) {
        processingThreads_.emplace_back(std::make_unique<std::thread>(&EventDispatcher::ProcessEvents, this));
    }
}

void EventDispatcher::StopProcessing() {
    if (!processing_) {
        return;
    }
    
    shouldStop_ = true;
    processing_ = false;
    
    queueCondition_.notify_all();
    
    for (auto& thread : processingThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    
    processingThreads_.clear();
}

void EventDispatcher::SetProcessingThreadCount(int count) {
    if (processing_) {
        StopProcessing();
        processingThreads_.clear();
        
        for (int i = 0; i < count; ++i) {
            processingThreads_.emplace_back(std::make_unique<std::thread>(&EventDispatcher::ProcessEvents, this));
        }
        
        processing_ = true;
        shouldStop_ = false;
    }
}

void EventDispatcher::ProcessEvents() {
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        queueCondition_.wait(lock, [this] {
            return !eventQueue_.empty() || shouldStop_;
        });
        
        if (shouldStop_) {
            break;
        }
        
        if (!eventQueue_.empty()) {
            QueuedEvent queuedEvent = eventQueue_.top();
            eventQueue_.pop();
            lock.unlock();
            
            try {
                DispatchToListeners(*queuedEvent.event);
            } catch (const std::exception& e) {
                // Логирование ошибки
                std::cerr << "Error processing event: " << e.what() << std::endl;
            }
        }
    }
}

void EventDispatcher::DispatchToListeners(const Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = listeners_.find(event.GetType());
    if (it != listeners_.end()) {
        for (const auto& listener : it->second) {
            try {
                listener(event);
                
                // Если событие было обработано, прекращаем дальнейшую обработку
                if (event.IsHandled()) {
                    break;
                }
            } catch (const std::exception& e) {
                // Логирование ошибки
                std::cerr << "Error in event listener: " << e.what() << std::endl;
            }
        }
    }
}

} // namespace events
} // namespace window_winapi
