#pragma once
#include "defines.h"
#include "core/assert.h"
#include <vulkan/vulkan.h>


#define VK_CHECK(expr)  \
{ \
    P_ASSERT(expr == VK_SUCCESS); \
}

typedef struct vulkan_swapchain_support_info {
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR* formats;
    u32 present_mode_count;
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

// Encapsulates both the physical device and logical device
typedef struct vulkan_device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;

    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    /// NOTE: put compute shader index here if needed

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
} vulkan_device;


typedef struct vulkan_context {
  VkInstance instance;
  VkAllocationCallbacks *allocator; // custom allocator we can use
                                    // this will be NULL for now to 
                                    // use the deiver's default allocator
  VkSurfaceKHR surface;
#if defined(_DEBUG)
  VkDebugUtilsMessengerEXT debug_messenger;
#endif /* _DEBUG */

  vulkan_device device;
} vulkan_context;
