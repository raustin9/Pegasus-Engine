#pragma once
#include "defines.h"

#include <vulkan/vulkan.h>

typedef struct vulkan_context {
  VkInstance instance;
  VkAllocationCallbacks *allocator; // custom allocator we can use
                                    // this will be NULL for now to 
                                    // use the deiver's default allocator
} vulkan_context;