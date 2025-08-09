#include "capture/frame_capture.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace WxeUI {
namespace Capture {

#ifdef _WIN32
struct FrameCapture::Win32CaptureData {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging_texture = nullptr;
    DXGI_OUTPUT_DESC output_desc = {};
    bool initialized = false;
};
#endif

// =============================================================================
// FrameCapture Implementation
// =============================================================================

FrameCapture::FrameCapture(const Config& config) 
    : config_(config), window_handle_(nullptr), region_set_(false) {
#ifdef _WIN32
    win32_data_ = std::make_unique<Win32CaptureData>();
#endif
    
    // Инициализируем буферы
    frame_buffers_.resize(config_.buffer_size);
    for (auto& buffer : frame_buffers_) {
        buffer = std::make_unique<FrameBuffer>();
    }
    
    // Заполняем очередь доступных буферов
    for (auto& buffer : frame_buffers_) {
        available_buffers_.push(std::move(buffer));
    }
    
    stats_.start_time = std::chrono::steady_clock::now();
}

FrameCapture::~FrameCapture() {
    Shutdown();
}

bool FrameCapture::Initialize(void* window_handle) {
    if (initialized_) {
        return true;
    }
    
    window_handle_ = window_handle;
    
    if (!InitializePlatform()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

void FrameCapture::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    StopCapture();
    ShutdownPlatform();
    
    initialized_ = false;
}

bool FrameCapture::StartCapture() {
    if (!initialized_ || capturing_) {
        return false;
    }
    
    capturing_ = true;
    workers_running_ = true;
    
    // Запускаем worker threads
    worker_threads_.resize(config_.worker_threads);
    for (auto& thread : worker_threads_) {
        thread = std::thread(&FrameCapture::WorkerThread, this);
    }
    
    return true;
}

bool FrameCapture::StopCapture() {
    if (!capturing_) {
        return false;
    }
    
    capturing_ = false;
    workers_running_ = false;
    
    // Уведомляем worker threads
    buffer_cv_.notify_all();
    
    // Ждем завершения worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    return true;
}

bool FrameCapture::IsCapturing() const {
    return capturing_;
}

bool FrameCapture::CaptureFrame(std::vector<uint8_t>& frame_data, FrameInfo& info) {
    if (!initialized_) {
        return false;
    }
    
    auto buffer = GetAvailableBuffer();
    if (!buffer) {
        return false;
    }
    
    bool success;
    if (region_set_) {
        success = CaptureFrameInternalRegion(*buffer, capture_region_);
    } else {
        success = CaptureFrameInternal(*buffer);
    }
    
    if (success) {
        frame_data = std::move(buffer->data);
        info = buffer->info;
        UpdateStats(info);
    }
    
    ReturnBuffer(std::move(buffer));
    return success;
}

bool FrameCapture::CaptureFrame(std::vector<uint8_t>& frame_data, FrameInfo& info, const CaptureRegion& region) {
    if (!initialized_) {
        return false;
    }
    
    auto buffer = GetAvailableBuffer();
    if (!buffer) {
        return false;
    }
    
    bool success = CaptureFrameInternalRegion(*buffer, region);
    
    if (success) {
        frame_data = std::move(buffer->data);
        info = buffer->info;
        UpdateStats(info);
    }
    
    ReturnBuffer(std::move(buffer));
    return success;
}

bool FrameCapture::CaptureFrameScaled(std::vector<uint8_t>& frame_data, FrameInfo& info, const ScaleParams& scale_params) {
    std::vector<uint8_t> original_data;
    FrameInfo original_info;
    
    if (!CaptureFrame(original_data, original_info)) {
        return false;
    }
    
    return ScaleFrame(original_data, original_info, frame_data, info, scale_params);
}

void FrameCapture::SetFrameCallback(FrameCallback callback) {
    frame_callback_ = callback;
}

void FrameCapture::SetErrorCallback(ErrorCallback callback) {
    error_callback_ = callback;
}

void FrameCapture::SetCaptureRegion(const CaptureRegion& region) {
    capture_region_ = region;
    region_set_ = true;
}

CaptureRegion FrameCapture::GetCaptureRegion() const {
    return capture_region_;
}

void FrameCapture::ResetCaptureRegion() {
    region_set_ = false;
}

CaptureStats FrameCapture::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void FrameCapture::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = CaptureStats();
    stats_.start_time = std::chrono::steady_clock::now();
}

bool FrameCapture::SaveFrame(const std::vector<uint8_t>& frame_data, const FrameInfo& info, 
                            const std::string& filename, CompressionFormat format) {
    if (format == CompressionFormat::NONE) {
        // Сохраняем raw данные
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(frame_data.data()), frame_data.size());
        return file.good();
    }
    
    // Сжимаем и сохраняем
    auto compressed_data = CompressFrame(frame_data, info, format);
    if (compressed_data.empty()) {
        return false;
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
    return file.good();
}

std::vector<uint8_t> FrameCapture::CompressFrame(const std::vector<uint8_t>& frame_data, 
                                                const FrameInfo& info, CompressionFormat format) {
    switch (format) {
        case CompressionFormat::PNG:
            return CompressPNG(frame_data, info);
        case CompressionFormat::JPEG:
            return CompressJPEG(frame_data, info);
        case CompressionFormat::WEBP:
            return CompressWebP(frame_data, info);
        default:
            return frame_data;
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

uint32_t FrameCapture::GetBytesPerPixel(FrameFormat format) {
    switch (format) {
        case FrameFormat::RGBA8:
        case FrameFormat::BGRA8:
            return 4;
        case FrameFormat::RGB8:
        case FrameFormat::BGR8:
            return 3;
        case FrameFormat::RGBA16F:
            return 8;
        case FrameFormat::R11G11B10F:
            return 4;
        default:
            return 4;
    }
}

bool FrameCapture::IsHDRFormat(FrameFormat format) {
    return format == FrameFormat::RGBA16F || format == FrameFormat::R11G11B10F;
}

std::string FrameCapture::GetFormatName(FrameFormat format) {
    switch (format) {
        case FrameFormat::RGBA8: return "RGBA8";
        case FrameFormat::BGRA8: return "BGRA8";
        case FrameFormat::RGB8: return "RGB8";
        case FrameFormat::BGR8: return "BGR8";
        case FrameFormat::RGBA16F: return "RGBA16F";
        case FrameFormat::R11G11B10F: return "R11G11B10F";
        default: return "Unknown";
    }
}

std::vector<FrameFormat> FrameCapture::GetSupportedFormats() {
    return {
        FrameFormat::RGBA8,
        FrameFormat::BGRA8,
        FrameFormat::RGB8,
        FrameFormat::BGR8,
        FrameFormat::RGBA16F,
        FrameFormat::R11G11B10F
    };
}

// =============================================================================
// Private Methods
// =============================================================================

#ifdef _WIN32
bool FrameCapture::InitializePlatform() {
    HRESULT hr;
    
    // Создаем D3D11 device
    D3D_FEATURE_LEVEL feature_level;
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &win32_data_->device,
        &feature_level,
        &win32_data_->context
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    // Получаем DXGI adapter
    IDXGIDevice* dxgi_device = nullptr;
    hr = win32_data_->device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
    if (FAILED(hr)) {
        return false;
    }
    
    IDXGIAdapter* adapter = nullptr;
    hr = dxgi_device->GetAdapter(&adapter);
    dxgi_device->Release();
    if (FAILED(hr)) {
        return false;
    }
    
    // Получаем output
    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr)) {
        return false;
    }
    
    // Получаем output1 для duplication
    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) {
        return false;
    }
    
    // Получаем description
    hr = output1->GetDesc(&win32_data_->output_desc);
    if (FAILED(hr)) {
        output1->Release();
        return false;
    }
    
    // Создаем duplication
    hr = output1->DuplicateOutput(win32_data_->device, &win32_data_->duplication);
    output1->Release();
    if (FAILED(hr)) {
        return false;
    }
    
    // Создаем staging texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = win32_data_->output_desc.DesktopCoordinates.right - win32_data_->output_desc.DesktopCoordinates.left;
    desc.Height = win32_data_->output_desc.DesktopCoordinates.bottom - win32_data_->output_desc.DesktopCoordinates.top;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    hr = win32_data_->device->CreateTexture2D(&desc, nullptr, &win32_data_->staging_texture);
    if (FAILED(hr)) {
        return false;
    }
    
    win32_data_->initialized = true;
    return true;
}

void FrameCapture::ShutdownPlatform() {
    if (!win32_data_->initialized) {
        return;
    }
    
    if (win32_data_->staging_texture) {
        win32_data_->staging_texture->Release();
        win32_data_->staging_texture = nullptr;
    }
    
    if (win32_data_->duplication) {
        win32_data_->duplication->Release();
        win32_data_->duplication = nullptr;
    }
    
    if (win32_data_->context) {
        win32_data_->context->Release();
        win32_data_->context = nullptr;
    }
    
    if (win32_data_->device) {
        win32_data_->device->Release();
        win32_data_->device = nullptr;
    }
    
    win32_data_->initialized = false;
}

bool FrameCapture::CaptureFrameInternal(FrameBuffer& buffer) {
    if (!win32_data_->initialized) {
        return false;
    }
    
    HRESULT hr;
    IDXGIResource* desktop_resource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    
    // Получаем новый кадр
    hr = win32_data_->duplication->AcquireNextFrame(100, &frame_info, &desktop_resource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return false; // Нет нового кадра
        }
        return false;
    }
    
    // Получаем texture
    ID3D11Texture2D* desktop_texture = nullptr;
    hr = desktop_resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktop_texture);
    desktop_resource->Release();
    if (FAILED(hr)) {
        win32_data_->duplication->ReleaseFrame();
        return false;
    }
    
    // Копируем в staging texture
    win32_data_->context->CopyResource(win32_data_->staging_texture, desktop_texture);
    desktop_texture->Release();
    
    // Читаем данные
    D3D11_MAPPED_SUBRESOURCE mapped_resource;
    hr = win32_data_->context->Map(win32_data_->staging_texture, 0, D3D11_MAP_READ, 0, &mapped_resource);
    if (FAILED(hr)) {
        win32_data_->duplication->ReleaseFrame();
        return false;
    }
    
    // Заполняем buffer
    uint32_t width = win32_data_->output_desc.DesktopCoordinates.right - win32_data_->output_desc.DesktopCoordinates.left;
    uint32_t height = win32_data_->output_desc.DesktopCoordinates.bottom - win32_data_->output_desc.DesktopCoordinates.top;
    uint32_t bytes_per_pixel = GetBytesPerPixel(config_.preferred_format);
    
    buffer.info.width = width;
    buffer.info.height = height;
    buffer.info.format = config_.preferred_format;
    buffer.info.stride = mapped_resource.RowPitch;
    buffer.info.data_size = height * mapped_resource.RowPitch;
    buffer.info.timestamp = std::chrono::steady_clock::now();
    buffer.info.frame_id = stats_.frames_captured.load();
    
    buffer.data.resize(buffer.info.data_size);
    memcpy(buffer.data.data(), mapped_resource.pData, buffer.info.data_size);
    
    win32_data_->context->Unmap(win32_data_->staging_texture, 0);
    win32_data_->duplication->ReleaseFrame();
    
    return true;
}
#else
bool FrameCapture::InitializePlatform() {
    // Заглушка для других платформ
    return false;
}

void FrameCapture::ShutdownPlatform() {
    // Заглушка для других платформ
}

bool FrameCapture::CaptureFrameInternal(FrameBuffer& buffer) {
    // Заглушка для других платформ
    return false;
}
#endif

bool FrameCapture::CaptureFrameInternalRegion(FrameBuffer& buffer, const CaptureRegion& region) {
    // Сначала захватываем весь экран
    if (!CaptureFrameInternal(buffer)) {
        return false;
    }
    
    // Затем обрезаем до нужного региона
    // Упрощенная реализация - в реальности нужно учитывать stride и format
    uint32_t bytes_per_pixel = GetBytesPerPixel(buffer.info.format);
    
    std::vector<uint8_t> cropped_data;
    cropped_data.reserve(region.width * region.height * bytes_per_pixel);
    
    for (uint32_t y = 0; y < region.height; ++y) {
        uint32_t src_y = region.y + y;
        if (src_y >= buffer.info.height) break;
        
        for (uint32_t x = 0; x < region.width; ++x) {
            uint32_t src_x = region.x + x;
            if (src_x >= buffer.info.width) break;
            
            size_t src_offset = (src_y * buffer.info.width + src_x) * bytes_per_pixel;
            size_t dst_offset = (y * region.width + x) * bytes_per_pixel;
            
            if (src_offset + bytes_per_pixel <= buffer.data.size()) {
                cropped_data.insert(cropped_data.end(), 
                                   buffer.data.begin() + src_offset,
                                   buffer.data.begin() + src_offset + bytes_per_pixel);
            }
        }
    }
    
    buffer.data = std::move(cropped_data);
    buffer.info.width = region.width;
    buffer.info.height = region.height;
    buffer.info.data_size = buffer.data.size();
    
    return true;
}

void FrameCapture::WorkerThread() {
    while (workers_running_) {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        
        // Ждем доступный буфер
        if (ready_buffers_.empty()) {
            if (config_.async_capture && capturing_) {
                // Захватываем новый кадр
                auto buffer = GetAvailableBuffer();
                if (buffer) {
                    bool success;
                    if (region_set_) {
                        success = CaptureFrameInternalRegion(*buffer, capture_region_);
                    } else {
                        success = CaptureFrameInternal(*buffer);
                    }
                    
                    if (success) {
                        UpdateStats(buffer->info);
                        ready_buffers_.push(std::move(buffer));
                    } else {
                        ReturnBuffer(std::move(buffer));
                    }
                }
            } else {
                buffer_cv_.wait_for(lock, std::chrono::milliseconds(16)); // ~60 FPS
                continue;
            }
        }
        
        // Обрабатываем готовый буфер
        if (!ready_buffers_.empty()) {
            auto buffer = std::move(ready_buffers_.front());
            ready_buffers_.pop();
            lock.unlock();
            
            ProcessFrame(std::move(buffer));
        }
    }
}

void FrameCapture::ProcessFrame(std::unique_ptr<FrameBuffer> buffer) {
    if (frame_callback_) {
        try {
            frame_callback_(buffer->info, buffer->data);
        } catch (const std::exception& e) {
            if (error_callback_) {
                error_callback_("Frame callback error: " + std::string(e.what()));
            }
        }
    }
    
    ReturnBuffer(std::move(buffer));
}

bool FrameCapture::ScaleFrame(const std::vector<uint8_t>& src_data, const FrameInfo& src_info,
                             std::vector<uint8_t>& dst_data, FrameInfo& dst_info,
                             const ScaleParams& params) {
    // Упрощенная реализация масштабирования (nearest neighbor)
    dst_info = src_info;
    dst_info.width = params.target_width;
    dst_info.height = params.target_height;
    dst_info.stride = params.target_width * GetBytesPerPixel(src_info.format);
    dst_info.data_size = dst_info.stride * params.target_height;
    
    dst_data.resize(dst_info.data_size);
    
    uint32_t bytes_per_pixel = GetBytesPerPixel(src_info.format);
    float x_ratio = static_cast<float>(src_info.width) / params.target_width;
    float y_ratio = static_cast<float>(src_info.height) / params.target_height;
    
    for (uint32_t y = 0; y < params.target_height; ++y) {
        for (uint32_t x = 0; x < params.target_width; ++x) {
            uint32_t src_x = static_cast<uint32_t>(x * x_ratio);
            uint32_t src_y = static_cast<uint32_t>(y * y_ratio);
            
            if (src_x < src_info.width && src_y < src_info.height) {
                size_t src_offset = (src_y * src_info.width + src_x) * bytes_per_pixel;
                size_t dst_offset = (y * params.target_width + x) * bytes_per_pixel;
                
                if (src_offset + bytes_per_pixel <= src_data.size() && 
                    dst_offset + bytes_per_pixel <= dst_data.size()) {
                    memcpy(&dst_data[dst_offset], &src_data[src_offset], bytes_per_pixel);
                }
            }
        }
    }
    
    return true;
}

std::vector<uint8_t> FrameCapture::CompressPNG(const std::vector<uint8_t>& data, const FrameInfo& info) {
    // Заглушка - в реальности нужна libpng
    return data;
}

std::vector<uint8_t> FrameCapture::CompressJPEG(const std::vector<uint8_t>& data, const FrameInfo& info) {
    // Заглушка - в реальности нужна libjpeg
    return data;
}

std::vector<uint8_t> FrameCapture::CompressWebP(const std::vector<uint8_t>& data, const FrameInfo& info) {
    // Заглушка - в реальности нужна libwebp
    return data;
}

void FrameCapture::UpdateStats(const FrameInfo& info) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.frames_captured++;
    stats_.total_bytes += info.data_size;
    
    // Вычисляем FPS и среднее время кадра
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration<double>(now - stats_.start_time).count();
    
    if (uptime > 0) {
        stats_.avg_fps = static_cast<double>(stats_.frames_captured.load()) / uptime;
    }
    
    auto frame_duration = std::chrono::duration<double, std::milli>(now - info.timestamp).count();
    if (frame_duration > 0) {
        stats_.avg_frame_time_ms = frame_duration;
    }
}

std::unique_ptr<FrameCapture::FrameBuffer> FrameCapture::GetAvailableBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (available_buffers_.empty()) {
        return nullptr;
    }
    
    auto buffer = std::move(available_buffers_.front());
    available_buffers_.pop();
    buffer->in_use = true;
    
    return buffer;
}

void FrameCapture::ReturnBuffer(std::unique_ptr<FrameBuffer> buffer) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    buffer->in_use = false;
    buffer->data.clear();
    available_buffers_.push(std::move(buffer));
    
    buffer_cv_.notify_one();
}

void FrameCapture::UpdateConfig(const Config& new_config) {
    config_ = new_config;
}

} // namespace Capture
} // namespace WindowWinapi
