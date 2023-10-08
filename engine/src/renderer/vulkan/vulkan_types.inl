#pragma once
#include "defines.h"
#include "core/assert.h"
#include <vulkan/vulkan.h>


#define VK_CHECK(expr)  \
{ \
    P_ASSERT(expr == VK_SUCCESS); \
} 

typedef struct vulkan_context {
  VkInstance instance;
  VkAllocationCallbacks *allocator; // custom allocator we can use
                                    // this will be NULL for now to 
                                    // use the deiver's default allocator
#if defined(_DEBUG)
  VkDebugUtilsMessengerEXT debug_messenger;
#endif /* _DEBUG */
} vulkan_context;
