//
// Created by emd22 on 2022-02-20.
//

#include "renderer_device.h"

#include <cstring>
#include <algorithm>

#include "../../system/debug.h"

namespace hyperion {

RendererDevice::RendererDevice()
    : device(nullptr),
      physical(nullptr),
      surface(nullptr)
{
    static int x = 0;
    DebugLog(LogType::Debug, "Created RendererDevice [%d]\n", x++);
}

RendererDevice::~RendererDevice()
{
}

void RendererDevice::SetDevice(const VkDevice &device) {
    this->device = device;
}
void RendererDevice::SetPhysicalDevice(VkPhysicalDevice physical)
{
    this->physical = physical;
    this->renderer_features.SetPhysicalDevice(physical);
}
void RendererDevice::SetRenderSurface(const VkSurfaceKHR &surface) {
    this->surface = surface;
}
void RendererDevice::SetRequiredExtensions(const std::vector<const char *> &extensions) {
    this->required_extensions = extensions;
}

VkDevice RendererDevice::GetDevice() {
    AssertThrow(this->device != nullptr);
    return this->device;
}
VkPhysicalDevice RendererDevice::GetPhysicalDevice() {
    return this->physical;
}
VkSurfaceKHR RendererDevice::GetRenderSurface() {
    if (this->surface == nullptr) {
        DebugLog(LogType::Fatal, "Device render surface is null!\n");
        throw std::runtime_error("Device render surface not set");
    }
    return this->surface;
}
std::vector<const char *> RendererDevice::GetRequiredExtensions() {
    return this->required_extensions;
}

QueueFamilyIndices RendererDevice::FindQueueFamilies() {
    VkPhysicalDevice _physical_device = this->GetPhysicalDevice();
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(_physical_device, &queue_family_count, families.data());

    int index = 0;

    for (const auto &queue_family : families) {
        VkBool32 supports_presentation = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_physical_device, index, this->GetRenderSurface(), &supports_presentation);

        if (supports_presentation)
            indices.present_family = index;

        if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_presentation)
            indices.graphics_family = index;

        /* For the transfer bit, use a separate queue family than the graphics bit */
        if ((queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_presentation)
            indices.transfer_family = index;

        /* Some devices appear only to compute and are not graphical,
         * so we need to make sure it supports presenting to the user. */

        /* Everything was found, return the indices */
        if (indices.IsComplete())
            break;

        index++;
    }

    if (!indices.IsComplete()) {
        /* if a graphics family has been found but a transfer family has /not/ been found,
         * set the transfer family to just use the graphics family. */

        if (indices.graphics_family.has_value() && !indices.transfer_family.has_value()) {
            DebugLog(LogType::Info, "No dedicated transfer family, using graphics family.\n");

            indices.transfer_family = indices.graphics_family;
        }
    }

    return indices;
}

std::vector<VkExtensionProperties> RendererDevice::GetSupportedExtensions() {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> supported_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, supported_extensions.data());
    return supported_extensions;
}

std::vector<const char *> RendererDevice::CheckExtensionSupport(std::vector<const char *> required_extensions) {
    auto extensions_supported = this->GetSupportedExtensions();
    std::vector<const char *> unsupported_extensions = required_extensions;

    for (const char *required_ext_name : required_extensions) {
        /* For each extension required, check */
        for (auto &supported : extensions_supported) {
            if (!strcmp(supported.extensionName, required_ext_name)) {
                unsupported_extensions.erase(std::find(unsupported_extensions.begin(), unsupported_extensions.end(), required_ext_name));
            }
        }
    }
    return unsupported_extensions;
}
std::vector<const char *> RendererDevice::CheckExtensionSupport() {
    return this->CheckExtensionSupport(this->GetRequiredExtensions());
}

SwapchainSupportDetails RendererDevice::QuerySwapchainSupport() {
    SwapchainSupportDetails details;
    VkPhysicalDevice _physical = this->GetPhysicalDevice();
    VkSurfaceKHR     _surface = this->GetRenderSurface();

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical, _surface, &details.capabilities);
    uint32_t count = 0;
    /* Get device surface formats */
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physical, _surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physical, _surface, &count, surface_formats.data());
    if (count == 0)
        DebugLog(LogType::Warn, "No surface formats available!\n");

    vkGetPhysicalDeviceSurfacePresentModesKHR(_physical, _surface, &count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physical, _surface, &count, present_modes.data());
    if (count == 0)
        DebugLog(LogType::Warn, "No present modes available!\n");

    details.formats = surface_formats;
    details.present_modes = present_modes;

    return details;
}

bool RendererDevice::CheckDeviceSuitable() {
    QueueFamilyIndices indices = this->FindQueueFamilies();
    std::vector<const char *> unsupported_extensions = this->CheckExtensionSupport();
    if (!unsupported_extensions.empty()) {
        DebugLog(LogType::Warn, "--- Unsupported Extensions ---\n");
        for (const auto &extension : unsupported_extensions) {
            DebugLog(LogType::Warn, "\t%s\n", extension);
        }
        DebugLog(LogType::Error, "Vulkan: Device does not support required extensions\n");
        /* TODO: try different device(s) before exploding  */
        throw std::runtime_error("Device does not support required extensions");
    }
    bool extensions_available = unsupported_extensions.empty();

    SwapchainSupportDetails swapchain_support = this->QuerySwapchainSupport();
    bool swapchains_available = (!swapchain_support.formats.empty() && !swapchain_support.present_modes.empty());

    return (indices.IsComplete() && extensions_available && swapchains_available);
}


VkDevice RendererDevice::CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions) {
    this->SetRequiredExtensions(required_extensions);

    std::vector<VkDeviceQueueCreateInfo> queue_create_info_vec;
    const float priorities[] = { 1.0f };
    // for each queue family(for separate threads) we add them to
    // our device initialization data
    for (uint32_t family : required_queue_families) {
        VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_info.pQueuePriorities = priorities;
        queue_info.queueCount = 1;
        queue_info.queueFamilyIndex = family;

        queue_create_info_vec.push_back(queue_info);
    }

    if (!this->CheckDeviceSuitable()) {
        DebugLog(LogType::Error, "Device not suitable!\n");
        throw std::runtime_error("Device not suitable");
    }

    VkDeviceCreateInfo create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    create_info.pQueueCreateInfos = queue_create_info_vec.data();
    create_info.queueCreateInfoCount = (uint32_t)(queue_create_info_vec.size());
    // Setup Device extensions
    create_info.enabledExtensionCount = (uint32_t)(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    // Setup Device Features
    create_info.pEnabledFeatures = &this->renderer_features.GetPhysicalDeviceFeatures();

    VkDevice _device;
    VkResult result = vkCreateDevice(this->physical, &create_info, nullptr, &_device);

    AssertThrowMsg(result == VK_SUCCESS, "Could not create RendererDevice!");

    this->SetDevice(_device);

    return this->device;
}

VkQueue RendererDevice::GetQueue(QueueFamilyIndices::Index_t queue_family_index, uint32_t queue_index) {
    VkQueue queue;
    vkGetDeviceQueue(this->GetDevice(), queue_family_index, queue_index, &queue);
    return queue;
}


void RendererDevice::Destroy() {
    /* By the time this destructor is called there should never
     * be a running queue, but just in case we will wait until
     * all the queues on our device are stopped. */
    vkDeviceWaitIdle(this->device);
    vkDestroyDevice(this->device, nullptr);
}

} // namespace hyperion