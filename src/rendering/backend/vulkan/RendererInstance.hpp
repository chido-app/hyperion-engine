#ifndef HYPERION_RENDERER_INSTANCE_H
#define HYPERION_RENDERER_INSTANCE_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include <system/SdlSystem.hpp>
#include <system/vma/VmaUsage.hpp>

#include "RendererDevice.hpp"
#include "RendererSwapchain.hpp"
#include "RendererShader.hpp"
#include "RendererBuffer.hpp"
#include "RendererGraphicsPipeline.hpp"
#include "RendererDescriptorSet.hpp"
#include "RendererFrame.hpp"
#include "RendererFrameHandler.hpp"
#include "RendererQueue.hpp"

#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

class Instance {
    static Result CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    static ExtensionMap GetExtensionMap();

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result CreateCommandPool(Queue &queue);

public:
    Instance(SystemSDL &_system, const char *app_name, const char *engine_name);
    Result Initialize(bool load_debug_layers = false);
    void CreateSurface();
    
    void WaitImageReady(Frame *frame);

    DescriptorPool &GetDescriptorPool()                   { return this->descriptor_pool; }
    const DescriptorPool &GetDescriptorPool() const       { return this->descriptor_pool; }
                                                          
    Queue &GetGraphicsQueue()                             { return this->queue_graphics; }
    const Queue &GetGraphicsQueue() const                 { return this->queue_graphics; }
    Queue &GetTransferQueue()                             { return this->queue_transfer; }
    const Queue &GetTransferQueue() const                 { return this->queue_transfer; }
    Queue &GetPresentQueue()                              { return this->queue_present; }
    const Queue &GetPresentQueue() const                  { return this->queue_present; }
    Queue &GetComputeQueue()                              { return this->queue_compute; }
    const Queue &GetComputeQueue() const                  { return this->queue_compute; }
                                                          
    VkCommandPool GetGraphicsCommandPool() const          { return this->queue_graphics.command_pool; }
    VkCommandPool GetComputeCommandPool() const           { return this->queue_compute.command_pool; }
    
    VkInstance GetInstance() const                        { return this->instance; }

    Swapchain *GetSwapchain()                             { return swapchain; }
    const Swapchain *GetSwapchain() const                 { return swapchain; }
                                                          
    FrameHandler *GetFrameHandler()                       { return frame_handler; }
    const FrameHandler *GetFrameHandler() const           { return frame_handler; }

    StagingBufferPool &GetStagingBufferPool()             { return m_staging_buffer_pool; }
    const StagingBufferPool &GetStagingBufferPool() const { return m_staging_buffer_pool; }

    VmaAllocator GetAllocator() const                     { return allocator; }

    helpers::SingleTimeCommands GetSingleTimeCommands();

    void SetValidationLayers(std::vector<const char *> _layers);

    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();

    void SetCurrentWindow(SystemWindow *window);

    SystemWindow *GetCurrentWindow();
    Result Destroy();

    const char *app_name;
    const char *engine_name;
    
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    FrameHandler *frame_handler;

private:

    SystemWindow   *window = nullptr;
    SystemSDL      system;

    VkInstance     instance = nullptr;
    VkSurfaceKHR   surface = nullptr;

    DescriptorPool descriptor_pool;

    VmaAllocator   allocator = nullptr;

    Device        *device = nullptr;

    Queue          queue_graphics,
                   queue_transfer,
                   queue_present,
                   queue_compute;
    
    std::vector<const char *> validation_layers;

    StagingBufferPool m_staging_buffer_pool;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE

