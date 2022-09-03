#include "RendererFeatures.hpp"

namespace hyperion {
namespace renderer {

Features::Features()
    : m_physical_device(nullptr),
      m_properties({}),
      m_features({})
{
    Memory::Set(static_cast<void *>(&dyn_functions), 0, sizeof(dyn_functions));
}

Features::Features(VkPhysicalDevice physical_device)
    : Features()
{
    SetPhysicalDevice(physical_device);
}

void Features::SetPhysicalDevice(VkPhysicalDevice physical_device)
{
    if ((m_physical_device = physical_device)) {
        vkGetPhysicalDeviceProperties(physical_device, &m_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &m_features);
        vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_properties);

#if HYP_FEATURES_ENABLE_RAYTRACING
        m_buffer_device_address_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = VK_NULL_HANDLE
        };
        
        m_raytracing_pipeline_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &m_buffer_device_address_features
        };
        
        m_acceleration_structure_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &m_raytracing_pipeline_features
        };

        m_multiview_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = &m_acceleration_structure_features
        };
#else
        m_multiview_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexing_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
            .pNext = &m_multiview_features
        };

        m_features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &m_indexing_features
        };

        vkGetPhysicalDeviceFeatures2(m_physical_device, &m_features2);
        
        // properties

#if HYP_FEATURES_ENABLE_RAYTRACING
        m_acceleration_structure_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = VK_NULL_HANDLE
        };

        m_raytracing_pipeline_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = &m_acceleration_structure_properties
        };

        m_sampler_minmax_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = &m_raytracing_pipeline_properties
        };
#else
        m_sampler_minmax_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexing_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
            .pNext = &m_sampler_minmax_properties
        };

        m_properties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &m_indexing_properties
        };

        vkGetPhysicalDeviceProperties2(m_physical_device, &m_properties2);
    }
}

void Features::LoadDynamicFunctions(Device *device)
{
#define HYP_LOAD_FN(name) do { \
        auto proc_addr = vkGetDeviceProcAddr(device->GetDevice(), #name); \
        AssertThrowMsg(proc_addr != nullptr, "Failed to load dynamic function " #name "\n"); \
        dyn_functions.name = reinterpret_cast<PFN_##name>(proc_addr); \
    } while (0)

#if HYP_FEATURES_ENABLE_RAYTRACING
    HYP_LOAD_FN(vkGetBufferDeviceAddressKHR); // currently only used for RT

    if (SupportsRaytracing() && !IsRaytracingDisabled()) {
        DebugLog(LogType::Debug, "Raytracing supported, loading raytracing-specific dynamic functions.\n");

        HYP_LOAD_FN(vkCmdBuildAccelerationStructuresKHR);
        HYP_LOAD_FN(vkBuildAccelerationStructuresKHR);
        HYP_LOAD_FN(vkCreateAccelerationStructureKHR);
        HYP_LOAD_FN(vkDestroyAccelerationStructureKHR);
        HYP_LOAD_FN(vkGetAccelerationStructureBuildSizesKHR);
        HYP_LOAD_FN(vkGetAccelerationStructureDeviceAddressKHR);
        HYP_LOAD_FN(vkCmdTraceRaysKHR);
        HYP_LOAD_FN(vkGetRayTracingShaderGroupHandlesKHR);
        HYP_LOAD_FN(vkCreateRayTracingPipelinesKHR);
    }
#endif

    //HYP_LOAD_FN(vkCmdDebugMarkerBeginEXT);
    //HYP_LOAD_FN(vkCmdDebugMarkerEndEXT);
    //HYP_LOAD_FN(vkCmdDebugMarkerInsertEXT);
    //HYP_LOAD_FN(vkDebugMarkerSetObjectNameEXT);

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    HYP_LOAD_FN(vkGetMoltenVKConfigurationMVK);
    HYP_LOAD_FN(vkSetMoltenVKConfigurationMVK);
#endif

#undef HYP_LOAD_FN
}

void Features::SetDeviceFeatures(Device *device)
{
#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    MVKConfiguration *mvk_config = nullptr;
    size_t sz = 1;
    dyn_functions.vkGetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvk_config, &sz);

    mvk_config = new MVKConfiguration[sz];

    for (size_t i = 0; i < sz; i++) {
#if defined(HYP_DEBUG_MODE) && HYP_DEBUG_MODE
        mvk_config[i].debugMode = true;
#endif
    }

    dyn_functions.vkSetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvk_config, &sz);

    delete[] mvk_config;
#endif
}

} // namespace renderer
} // namespace hyperion
