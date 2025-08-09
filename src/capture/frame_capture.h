#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

namespace WxeUI {
namespace Capture {

enum class FrameFormat {
    RGBA8,      // 32-bit RGBA
    BGRA8,      // 32-bit BGRA (Windows default)
    RGB8,       // 24-bit RGB
    BGR8,       // 24-bit BGR
    RGBA16F,    // 64-bit HDR RGBA
    R11G11B10F  // 32-bit HDR RGB
};

enum class CompressionFormat {
    NONE,       // Raw data
    PNG,        // Lossless PNG
    JPEG,       // Lossy JPEG
    WEBP,       // Modern WebP
    DDS,        // DirectX texture
    KTX2        // Khronos texture
};

struct FrameInfo {
    uint32_t width;
    uint32_t height;
    FrameFormat format;
    uint32_t stride;        // Bytes per row
    size_t data_size;       // Total size in bytes
    std::chrono::steady_clock::time_point timestamp;
    uint32_t frame_id;
    bool hdr_enabled;
    float dpi_scale;
    
    FrameInfo() : width(0), height(0), format(FrameFormat::BGRA8), 
                  stride(0), data_size(0), frame_id(0), 
                  hdr_enabled(false), dpi_scale(1.0f) {}
};

struct CaptureRegion {
    int32_t x, y;
    uint32_t width, height;
    
    CaptureRegion() : x(0), y(0), width(0), height(0) {}
    CaptureRegion(int32_t x_, int32_t y_, uint32_t w, uint32_t h) 
        : x(x_), y(y_), width(w), height(h) {}
};

struct ScaleParams {
    uint32_t target_width;
    uint32_t target_height;
    enum class Filter {
        NEAREST,    // Быстрая, без сглаживания
        LINEAR,     // Билинейная
        CUBIC,      // Бикубическая
        LANCZOS     // Высокое качество
    } filter;
    
    ScaleParams() : target_width(0), target_height(0), filter(Filter::LINEAR) {}
};

struct CaptureStats {
    std::atomic<uint64_t> frames_captured{0};
    std::atomic<uint64_t> frames_dropped{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<double> avg_fps{0.0};
    std::atomic<double> avg_frame_time_ms{0.0};
    std::chrono::steady_clock::time_point start_time;
    
    CaptureStats() : start_time(std::chrono::steady_clock::now()) {}
    
    double GetUptime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }
};

class FrameCapture {
public:
    struct Config {
        uint32_t max_fps = 60;              // Максимальный FPS
        uint32_t buffer_size = 3;           // Количество буферов
        bool enable_hdr = false;            // HDR захват
        bool enable_cursor = true;          // Захват курсора
        FrameFormat preferred_format = FrameFormat::BGRA8;
        CompressionFormat compression = CompressionFormat::NONE;
        
        // Оптимизация
        bool use_gpu_acceleration = true;   // GPU ускорение
        bool async_capture = true;          // Асинхронный захват
        uint32_t worker_threads = 2;        // Потоки для обработки
        
        // Quality settings
        int jpeg_quality = 85;              // JPEG качество (1-100)
        int webp_quality = 80;              // WebP качество (1-100)
        bool png_fast_compression = false;  // Быстрая PNG компрессия
    };
    
    using FrameCallback = std::function<void(const FrameInfo&, const std::vector<uint8_t>&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
public:
    explicit FrameCapture(const Config& config = Config{});
    ~FrameCapture();
    
    // Основные операции захвата
    bool Initialize(void* window_handle = nullptr);
    void Shutdown();
    
    bool StartCapture();
    bool StopCapture();
    bool IsCapturing() const;
    
    // Однократный захват
    bool CaptureFrame(std::vector<uint8_t>& frame_data, FrameInfo& info);
    bool CaptureFrame(std::vector<uint8_t>& frame_data, FrameInfo& info, const CaptureRegion& region);
    
    // Захват с масштабированием
    bool CaptureFrameScaled(std::vector<uint8_t>& frame_data, FrameInfo& info, 
                           const ScaleParams& scale_params);
    
    // Непрерывный захват с callback
    void SetFrameCallback(FrameCallback callback);
    void SetErrorCallback(ErrorCallback callback);
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
    // Регион захвата
    void SetCaptureRegion(const CaptureRegion& region);
    CaptureRegion GetCaptureRegion() const;
    void ResetCaptureRegion(); // Захват всего экрана
    
    // Статистика
    CaptureStats GetStats() const;
    void ResetStats();
    
    // Форматы и компрессия
    bool SaveFrame(const std::vector<uint8_t>& frame_data, const FrameInfo& info, 
                  const std::string& filename, CompressionFormat format = CompressionFormat::PNG);
    
    std::vector<uint8_t> CompressFrame(const std::vector<uint8_t>& frame_data, 
                                      const FrameInfo& info, CompressionFormat format);
    
    // Утилиты
    static uint32_t GetBytesPerPixel(FrameFormat format);
    static bool IsHDRFormat(FrameFormat format);
    static std::string GetFormatName(FrameFormat format);
    static std::vector<FrameFormat> GetSupportedFormats();
    
private:
    Config config_;
    
    // Состояние
    std::atomic<bool> initialized_{false};
    std::atomic<bool> capturing_{false};
    void* window_handle_;
    
    // Захват
    CaptureRegion capture_region_;
    bool region_set_;
    
    // Callbacks
    FrameCallback frame_callback_;
    ErrorCallback error_callback_;
    
    // Статистика
    mutable CaptureStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Буферы и threading
    struct FrameBuffer {
        std::vector<uint8_t> data;
        FrameInfo info;
        bool in_use;
        
        FrameBuffer() : in_use(false) {}
    };
    
    std::vector<std::unique_ptr<FrameBuffer>> frame_buffers_;
    std::queue<std::unique_ptr<FrameBuffer>> available_buffers_;
    std::queue<std::unique_ptr<FrameBuffer>> ready_buffers_;
    
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> workers_running_{false};
    
    // Platform-specific
#ifdef _WIN32
    struct Win32CaptureData;
    std::unique_ptr<Win32CaptureData> win32_data_;
#endif
    
    // Внутренние методы
    bool InitializePlatform();
    void ShutdownPlatform();
    
    bool CaptureFrameInternal(FrameBuffer& buffer);
    bool CaptureFrameInternalRegion(FrameBuffer& buffer, const CaptureRegion& region);
    
    void WorkerThread();
    void ProcessFrame(std::unique_ptr<FrameBuffer> buffer);
    
    // Обработка изображений
    bool ScaleFrame(const std::vector<uint8_t>& src_data, const FrameInfo& src_info,
                   std::vector<uint8_t>& dst_data, FrameInfo& dst_info,
                   const ScaleParams& params);
    
    bool ConvertFormat(const std::vector<uint8_t>& src_data, const FrameInfo& src_info,
                      std::vector<uint8_t>& dst_data, FrameInfo& dst_info,
                      FrameFormat target_format);
    
    // Компрессия
    std::vector<uint8_t> CompressPNG(const std::vector<uint8_t>& data, const FrameInfo& info);
    std::vector<uint8_t> CompressJPEG(const std::vector<uint8_t>& data, const FrameInfo& info);
    std::vector<uint8_t> CompressWebP(const std::vector<uint8_t>& data, const FrameInfo& info);
    
    // Утилиты
    void UpdateStats(const FrameInfo& info);
    std::unique_ptr<FrameBuffer> GetAvailableBuffer();
    void ReturnBuffer(std::unique_ptr<FrameBuffer> buffer);
};

} // namespace Capture
} // namespace WindowWinapi
