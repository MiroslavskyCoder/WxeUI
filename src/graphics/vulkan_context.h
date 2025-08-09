#pragma once

#include "window_winapi.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <vk_mem_alloc.h>
#include <memory>
#include <vector>
#include <optional>
#include <set>

namespace WxeUI {

class VulkanContext : public IGraphicsContext {
public:
    VulkanContext();
    virtual ~VulkanContext();
    
    bool Initialize(HWND hwnd, int width, int height) override;
    void Shutdown() override;
    void ResizeBuffers(int width, int height) override;
    void Present() override;
    void Clear(float r, float g, float b, float a) override;
    GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
    sk_sp<SkSurface> GetSkiaSurface() override;
    void WaitForGPU() override;
    
    // Vulkan специфичные методы
    VkInstance GetInstance() const { return instance_; }
    VkDevice GetDevice() const { return device_; }
    VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice_; }
    VkQueue GetGraphicsQueue() const { return graphicsQueue_; }
    VkCommandBuffer GetCurrentCommandBuffer() const { return commandBuffers_[currentFrame_]; }
    
    // Multi-GPU поддержка
    struct GPUInfo {
        VkPhysicalDevice device;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceMemoryProperties memoryProperties;
        std::vector<VkQueueFamilyProperties> queueFamilies;
        uint32_t score;
    };
    
    std::vector<GPUInfo> EnumerateGPUs() const;
    bool SelectOptimalGPU();
    
    // Memory management через VMA
    struct AllocationInfo {
        VmaAllocation allocation;
        VmaAllocationInfo info;
        VkBuffer buffer;
        VkImage image;
    };
    
    VkBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocation& allocation);
    VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocation& allocation);
    void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
    void DestroyImage(VkImage image, VmaAllocation allocation);
    
    // Validation layers для отладки
    void EnableValidationLayers(bool enable) { validationLayersEnabled_ = enable; }
    bool AreValidationLayersEnabled() const { return validationLayersEnabled_; }
    
private:
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    
    // Core Vulkan objects
    VkInstance instance_;
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    
    // Queues
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkQueue computeQueue_;
    uint32_t graphicsQueueFamily_;
    uint32_t presentQueueFamily_;
    uint32_t computeQueueFamily_;
    
    // Surface and swap chain
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapChain_;
    std::vector<VkImage> swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    
    // Command buffers
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    
    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_;
    
    // Validation layers
    VkDebugUtilsMessengerEXT debugMessenger_;
    bool validationLayersEnabled_;
    
    // VMA allocator
    VmaAllocator allocator_;
    
    // Skia integration
    sk_sp<GrDirectContext> grContext_;
    sk_sp<SkSurface> skiaSurface_;
    
    // Multi-GPU
    std::vector<GPUInfo> availableGPUs_;
    
    // Helper methods
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool CreateSurface(HWND hwnd);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapChain(int width, int height);
    bool CreateImageViews();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    bool CreateVMAAllocator();
    bool CreateSkiaContext();
    void CleanupSwapChain();
    void RecreateSwapChain(int width, int height);
    
    // Queue family helpers
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;
        
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    
    // Swap chain support
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height);
    
    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
    
    HWND hwnd_;
    int width_, height_;
};

} // namespace window_winapi
