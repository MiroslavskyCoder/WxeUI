#include "memory/memory_manager.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace WxeUI {
namespace Memory {

#ifdef _WIN32
struct MemoryManager::Win32MemoryData {
    PROCESS_MEMORY_COUNTERS_EX process_memory = {};
    MEMORYSTATUSEX global_memory = {};
    
    Win32MemoryData() {
        global_memory.dwLength = sizeof(MEMORYSTATUSEX);
    }
};
#endif

// =============================================================================
// MemoryPool Implementation
// =============================================================================

MemoryPool::MemoryPool(MemoryType type, const Config& config) 
    : type_(type), config_(config), pool_memory_(nullptr), pool_size_(0), used_size_(0) {
    // Выделяем начальный пул памяти
    pool_size_ = config_.initial_size;
    
#ifdef _WIN32
    if (type == MemoryType::SYSTEM_RAM) {
        pool_memory_ = VirtualAlloc(nullptr, pool_size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    } else {
        pool_memory_ = malloc(pool_size_);
    }
#else
    pool_memory_ = malloc(pool_size_);
#endif
    
    if (pool_memory_) {
        // Добавляем один большой свободный блок
        blocks_.emplace_back(pool_memory_, pool_size_);
    }
}

MemoryPool::~MemoryPool() {
    Clear();
    
    if (pool_memory_) {
#ifdef _WIN32
        if (type_ == MemoryType::SYSTEM_RAM) {
            VirtualFree(pool_memory_, 0, MEM_RELEASE);
        } else {
            free(pool_memory_);
        }
#else
        free(pool_memory_);
#endif
        pool_memory_ = nullptr;
    }
}

void* MemoryPool::Allocate(size_t size) {
    return AllocateAligned(size, config_.alignment);
}

void* MemoryPool::AllocateAligned(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    void* ptr = AllocateInternal(size, alignment);
    if (ptr) {
        stats_.total_allocations++;
        stats_.current_allocations++;
        stats_.total_bytes_allocated += size;
        stats_.current_bytes_allocated += size;
        
        // Обновляем пики
        if (stats_.current_allocations > stats_.peak_allocations) {
            stats_.peak_allocations = stats_.current_allocations.load();
        }
        if (stats_.current_bytes_allocated > stats_.peak_bytes_allocated) {
            stats_.peak_bytes_allocated = stats_.current_bytes_allocated.load();
        }
    } else {
        stats_.failed_allocations++;
    }
    
    return ptr;
}

bool MemoryPool::Deallocate(void* ptr) {
    if (!ptr) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Находим блок
    for (auto& block : blocks_) {
        if (block.ptr == ptr && block.in_use) {
            block.in_use = false;
            block.free_time = std::chrono::steady_clock::now();
            
            stats_.current_allocations--;
            stats_.current_bytes_allocated -= block.size;
            used_size_ -= block.size;
            
            // Объединяем соседние свободные блоки
            MergeAdjacentBlocks();
            
            return true;
        }
    }
    
    return false;
}

void MemoryPool::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& block : blocks_) {
        block.in_use = false;
    }
    
    used_size_ = 0;
    stats_.current_allocations = 0;
    stats_.current_bytes_allocated = 0;
    
    // Оставляем один большой свободный блок
    blocks_.clear();
    if (pool_memory_) {
        blocks_.emplace_back(pool_memory_, pool_size_);
    }
}

size_t MemoryPool::GetTotalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_size_;
}

size_t MemoryPool::GetUsedSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return used_size_;
}

size_t MemoryPool::GetFreeSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_size_ - used_size_;
}

size_t MemoryPool::GetFragmentation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_size_ == 0) {
        return 0;
    }
    
    size_t free_blocks = 0;
    size_t largest_free = 0;
    
    for (const auto& block : blocks_) {
        if (!block.in_use) {
            free_blocks++;
            largest_free = std::max(largest_free, block.size);
        }
    }
    
    size_t total_free = GetFreeSize();
    if (total_free == 0) {
        return 0;
    }
    
    // Простая метрика фрагментации: (количество_свободных_блоков - 1) / общая_свободная_память
    return free_blocks > 1 ? ((free_blocks - 1) * 100) / total_free : 0;
}

AllocationStats MemoryPool::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void MemoryPool::ResetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.Reset();
}

void* MemoryPool::AllocateInternal(size_t size, size_t alignment) {
    size_t aligned_size = GetAlignedSize(size, alignment);
    
    // Ищем подходящий свободный блок
    for (auto& block : blocks_) {
        if (!block.in_use && block.size >= aligned_size) {
            // Проверяем выравнивание
            uintptr_t addr = reinterpret_cast<uintptr_t>(block.ptr);
            size_t padding = (alignment - (addr % alignment)) % alignment;
            
            if (block.size >= aligned_size + padding) {
                // Разделяем блок если нужно
                if (block.size > aligned_size + padding + sizeof(Block)) {
                    size_t remaining_size = block.size - aligned_size - padding;
                    void* remaining_ptr = static_cast<char*>(block.ptr) + aligned_size + padding;
                    
                    blocks_.emplace_back(remaining_ptr, remaining_size);
                }
                
                block.size = aligned_size;
                block.in_use = true;
                block.alloc_time = std::chrono::steady_clock::now();
                used_size_ += aligned_size;
                
                return static_cast<char*>(block.ptr) + padding;
            }
        }
    }
    
    // Не найден подходящий блок - пытаемся расширить пул
    if (GrowPool(aligned_size)) {
        return AllocateInternal(size, alignment);
    }
    
    return nullptr;
}

bool MemoryPool::GrowPool(size_t min_additional_size) {
    size_t new_size = pool_size_;
    
    while (new_size - pool_size_ < min_additional_size) {
        new_size *= config_.growth_factor;
        
        if (new_size > config_.max_size) {
            new_size = config_.max_size;
            break;
        }
    }
    
    if (new_size <= pool_size_ || new_size > config_.max_size) {
        return false;
    }
    
    // Перевыделяем память
#ifdef _WIN32
    void* new_memory;
    if (type_ == MemoryType::SYSTEM_RAM) {
        new_memory = VirtualAlloc(nullptr, new_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    } else {
        new_memory = malloc(new_size);
    }
#else
    void* new_memory = malloc(new_size);
#endif
    
    if (!new_memory) {
        return false;
    }
    
    // Копируем существующие данные
    if (pool_memory_) {
        memcpy(new_memory, pool_memory_, pool_size_);
        
#ifdef _WIN32
        if (type_ == MemoryType::SYSTEM_RAM) {
            VirtualFree(pool_memory_, 0, MEM_RELEASE);
        } else {
            free(pool_memory_);
        }
#else
        free(pool_memory_);
#endif
    }
    
    // Обновляем указатели в блоках
    ptrdiff_t offset = static_cast<char*>(new_memory) - static_cast<char*>(pool_memory_);
    for (auto& block : blocks_) {
        block.ptr = static_cast<char*>(block.ptr) + offset;
    }
    
    // Добавляем новый свободный блок
    void* new_block_ptr = static_cast<char*>(new_memory) + pool_size_;
    size_t new_block_size = new_size - pool_size_;
    blocks_.emplace_back(new_block_ptr, new_block_size);
    
    pool_memory_ = new_memory;
    pool_size_ = new_size;
    
    return true;
}

size_t MemoryPool::GetAlignedSize(size_t size, size_t alignment) const {
    return (size + alignment - 1) & ~(alignment - 1);
}

void MemoryPool::MergeAdjacentBlocks() {
    // Сортируем блоки по адресу
    std::sort(blocks_.begin(), blocks_.end(), [](const Block& a, const Block& b) {
        return a.ptr < b.ptr;
    });
    
    // Объединяем соседние свободные блоки
    for (size_t i = 0; i < blocks_.size() - 1; ) {
        if (!blocks_[i].in_use && !blocks_[i + 1].in_use) {
            // Проверяем, что блоки соседние
            char* end_of_first = static_cast<char*>(blocks_[i].ptr) + blocks_[i].size;
            if (end_of_first == blocks_[i + 1].ptr) {
                // Объединяем блоки
                blocks_[i].size += blocks_[i + 1].size;
                blocks_.erase(blocks_.begin() + i + 1);
                continue;
            }
        }
        ++i;
    }
}

// =============================================================================
// MemoryManager Implementation
// =============================================================================

MemoryManager::MemoryManager(const Config& config) : config_(config) {
#ifdef _WIN32
    win32_data_ = std::make_unique<Win32MemoryData>();
#endif
}

MemoryManager::~MemoryManager() {
    Shutdown();
}

bool MemoryManager::Initialize() {
    if (!InitializePlatform()) {
        return false;
    }
    
    if (config_.enable_memory_pools) {
        // Создаем пулы памяти
        memory_pools_[MemoryType::SYSTEM_RAM] = std::make_unique<MemoryPool>(
            MemoryType::SYSTEM_RAM, 
            MemoryPool::Config{
                .initial_size = config_.small_pool_size,
                .max_size = config_.large_pool_size
            }
        );
        
        memory_pools_[MemoryType::GPU_VRAM] = std::make_unique<MemoryPool>(
            MemoryType::GPU_VRAM,
            MemoryPool::Config{
                .initial_size = config_.medium_pool_size,
                .max_size = config_.large_pool_size
            }
        );
    }
    
    if (config_.enable_auto_cleanup || config_.enable_prediction) {
        StartMonitoring();
    }
    
    return true;
}

void MemoryManager::Shutdown() {
    StopMonitoring();
    ShutdownPlatform();
    
    memory_pools_.clear();
    cleanup_callbacks_.clear();
}

MemoryInfo MemoryManager::GetMemoryInfo(MemoryType type) const {
    switch (type) {
        case MemoryType::SYSTEM_RAM:
            return GetSystemMemoryInfo();
        case MemoryType::GPU_VRAM:
            return GetGPUMemoryInfo();
        default:
            return MemoryInfo{};
    }
}

std::unordered_map<MemoryType, MemoryInfo> MemoryManager::GetAllMemoryInfo() const {
    std::unordered_map<MemoryType, MemoryInfo> info_map;
    
    info_map[MemoryType::SYSTEM_RAM] = GetSystemMemoryInfo();
    info_map[MemoryType::GPU_VRAM] = GetGPUMemoryInfo();
    
    return info_map;
}

void* MemoryManager::AllocateFromPool(size_t size, MemoryType type) {
    auto it = memory_pools_.find(type);
    if (it != memory_pools_.end()) {
        return it->second->Allocate(size);
    }
    
    // Fallback на обычное выделение
    return malloc(size);
}

bool MemoryManager::DeallocateFromPool(void* ptr, MemoryType type) {
    auto it = memory_pools_.find(type);
    if (it != memory_pools_.end()) {
        return it->second->Deallocate(ptr);
    }
    
    // Fallback на обычное освобождение
    free(ptr);
    return true;
}

size_t MemoryManager::FreeUnusedMemory() {
    size_t freed = 0;
    
    // Вызываем callbacks для освобождения памяти
    for (const auto& pair : cleanup_callbacks_) {
        try {
            freed += pair.second();
        } catch (...) {
            // Игнорируем ошибки cleanup callbacks
        }
    }
    
    // Сжимаем пулы памяти
    for (auto& pair : memory_pools_) {
        if (pair.second && config_.enable_memory_pools) {
            pair.second->Shrink();
        }
    }
    
    return freed;
}

size_t MemoryManager::ForceCleanup() {
    return ExecuteCleanup();
}

bool MemoryManager::RequestMemory(size_t size, MemoryType type) {
    MemoryInfo info = GetMemoryInfo(type);
    
    if (info.available_bytes >= size) {
        return true;
    }
    
    // Пытаемся освободить память
    size_t freed = FreeUnusedMemory();
    
    info = GetMemoryInfo(type);
    return info.available_bytes >= size;
}

void MemoryManager::StartMonitoring() {
    if (monitoring_active_) {
        return;
    }
    
    monitoring_active_ = true;
    monitor_thread_ = std::thread(&MemoryManager::MonitorThread, this);
}

void MemoryManager::StopMonitoring() {
    if (!monitoring_active_) {
        return;
    }
    
    monitoring_active_ = false;
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

bool MemoryManager::IsMonitoring() const {
    return monitoring_active_;
}

void MemoryManager::RegisterCleanupCallback(const std::string& name, CleanupCallback callback) {
    cleanup_callbacks_[name] = callback;
}

void MemoryManager::UnregisterCleanupCallback(const std::string& name) {
    cleanup_callbacks_.erase(name);
}

void MemoryManager::SetWarningCallback(WarningCallback callback) {
    warning_callback_ = callback;
}

size_t MemoryManager::PredictMemoryUsage(MemoryType type, std::chrono::seconds future_time) const {
    if (!config_.enable_prediction) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    auto it = usage_history_.find(type);
    if (it == usage_history_.end() || it->second.size() < 2) {
        return 0;
    }
    
    double growth_rate = CalculateGrowthRate(type);
    size_t current_usage = it->second.back().used_bytes;
    
    // Простая линейная экстраполяция
    double seconds = future_time.count();
    size_t predicted = static_cast<size_t>(current_usage + (growth_rate * seconds));
    
    return predicted;
}

bool MemoryManager::WillExceedThreshold(MemoryType type, std::chrono::seconds future_time, double threshold) const {
    size_t predicted = PredictMemoryUsage(type, future_time);
    MemoryInfo info = GetMemoryInfo(type);
    
    if (info.total_bytes == 0) {
        return false;
    }
    
    double predicted_percentage = static_cast<double>(predicted) / info.total_bytes * 100.0;
    return predicted_percentage > threshold;
}

AllocationStats MemoryManager::GetPoolStats(MemoryType type) const {
    auto it = memory_pools_.find(type);
    if (it != memory_pools_.end()) {
        return it->second->GetStats();
    }
    
    return AllocationStats{};
}

void MemoryManager::ResetAllStats() {
    for (auto& pair : memory_pools_) {
        if (pair.second) {
            pair.second->ResetStats();
        }
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    usage_history_.clear();
}

void MemoryManager::UpdateConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    config_ = new_config;
}

// =============================================================================
// Private Methods
// =============================================================================

#ifdef _WIN32
bool MemoryManager::InitializePlatform() {
    return true;
}

void MemoryManager::ShutdownPlatform() {
    // Ничего особенного не требуется
}

MemoryInfo MemoryManager::GetSystemMemoryInfo() const {
    MEMORYSTATUSEX mem_status = {};
    mem_status.dwLength = sizeof(MEMORYSTATUSEX);
    
    MemoryInfo info;
    
    if (GlobalMemoryStatusEx(&mem_status)) {
        info.total_bytes = mem_status.ullTotalPhys;
        info.available_bytes = mem_status.ullAvailPhys;
        info.used_bytes = info.total_bytes - info.available_bytes;
        info.usage_percentage = static_cast<double>(info.used_bytes) / info.total_bytes * 100.0;
    }
    
    return info;
}

MemoryInfo MemoryManager::GetGPUMemoryInfo() const {
    // Упрощенная реализация - в реальности нужно использовать DXGI или NVML
    MemoryInfo info;
    info.total_bytes = 1024 * 1024 * 1024; // 1GB предполагаемая VRAM
    info.available_bytes = info.total_bytes * 0.7; // 70% доступно
    info.used_bytes = info.total_bytes - info.available_bytes;
    info.usage_percentage = static_cast<double>(info.used_bytes) / info.total_bytes * 100.0;
    
    return info;
}
#else
bool MemoryManager::InitializePlatform() {
    return true;
}

void MemoryManager::ShutdownPlatform() {
    // Заглушка для других платформ
}

MemoryInfo MemoryManager::GetSystemMemoryInfo() const {
    // Заглушка для других платформ
    return MemoryInfo{};
}

MemoryInfo MemoryManager::GetGPUMemoryInfo() const {
    // Заглушка для других платформ
    return MemoryInfo{};
}
#endif

void MemoryManager::MonitorThread() {
    while (monitoring_active_) {
        try {
            CheckMemoryLevels();
            
            if (config_.enable_prediction) {
                // Записываем текущее использование памяти
                RecordMemoryUsage(MemoryType::SYSTEM_RAM, GetSystemMemoryInfo().used_bytes);
                RecordMemoryUsage(MemoryType::GPU_VRAM, GetGPUMemoryInfo().used_bytes);
            }
            
            std::this_thread::sleep_for(config_.monitor_interval);
        } catch (...) {
            // Игнорируем ошибки мониторинга
        }
    }
}

void MemoryManager::CheckMemoryLevels() {
    auto all_info = GetAllMemoryInfo();
    
    for (const auto& pair : all_info) {
        MemoryType type = pair.first;
        const MemoryInfo& info = pair.second;
        
        if (info.usage_percentage > config_.critical_threshold) {
            if (warning_callback_) {
                warning_callback_(type, info.usage_percentage);
            }
            
            if (config_.enable_auto_cleanup) {
                ExecuteCleanup();
            }
        } else if (info.usage_percentage > config_.warning_threshold) {
            if (warning_callback_) {
                warning_callback_(type, info.usage_percentage);
            }
        }
        
        if (info.usage_percentage > config_.cleanup_threshold && config_.enable_auto_cleanup) {
            ExecuteCleanup();
        }
    }
}

size_t MemoryManager::ExecuteCleanup() {
    size_t total_freed = 0;
    
    for (const auto& pair : cleanup_callbacks_) {
        try {
            size_t freed = pair.second();
            total_freed += freed;
        } catch (...) {
            // Игнорируем ошибки отдельных callbacks
        }
    }
    
    return total_freed;
}

void MemoryManager::RecordMemoryUsage(MemoryType type, size_t used_bytes) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    auto& history = usage_history_[type];
    
    MemoryUsagePoint point;
    point.timestamp = std::chrono::steady_clock::now();
    point.used_bytes = used_bytes;
    
    history.push_back(point);
    
    // Ограничиваем размер истории
    auto cutoff_time = point.timestamp - std::chrono::seconds(config_.prediction_window);
    history.erase(
        std::remove_if(history.begin(), history.end(),
                      [cutoff_time](const MemoryUsagePoint& p) {
                          return p.timestamp < cutoff_time;
                      }),
        history.end()
    );
}

double MemoryManager::CalculateGrowthRate(MemoryType type) const {
    auto it = usage_history_.find(type);
    if (it == usage_history_.end() || it->second.size() < 2) {
        return 0.0;
    }
    
    const auto& history = it->second;
    
    // Простой линейный тренд
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    size_t n = history.size();
    
    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(history[i].used_bytes);
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    // Вычисляем коэффициент наклона (байты в секунду)
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Преобразуем в байты в секунду
    auto time_span = std::chrono::duration_cast<std::chrono::seconds>(
        history.back().timestamp - history.front().timestamp
    ).count();
    
    return time_span > 0 ? slope / time_span : 0.0;
}

// =============================================================================
// GlobalMemoryManager Implementation
// =============================================================================

std::unique_ptr<MemoryManager> GlobalMemoryManager::instance_;
std::mutex GlobalMemoryManager::instance_mutex_;

MemoryManager& GlobalMemoryManager::Instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!instance_) {
        instance_ = std::make_unique<MemoryManager>();
        instance_->Initialize();
    }
    
    return *instance_;
}

void GlobalMemoryManager::Initialize(const MemoryManager::Config& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (instance_) {
        instance_->Shutdown();
    }
    
    instance_ = std::make_unique<MemoryManager>(config);
    instance_->Initialize();
}

void GlobalMemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (instance_) {
        instance_->Shutdown();
        instance_.reset();
    }
}

void* GlobalMemoryManager::Allocate(size_t size, MemoryType type) {
    return Instance().AllocateFromPool(size, type);
}

void GlobalMemoryManager::Deallocate(void* ptr, MemoryType type) {
    Instance().DeallocateFromPool(ptr, type);
}

MemoryInfo GlobalMemoryManager::GetMemoryInfo(MemoryType type) {
    return Instance().GetMemoryInfo(type);
}

} // namespace Memory
} // namespace WindowWinapi
