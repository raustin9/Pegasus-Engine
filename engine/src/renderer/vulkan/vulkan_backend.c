#include "renderer/vulkan/vulkan_backend.h"
#include "vulkan_types.inl"
#include "core/logger.h"

static vulkan_context context;

b8 
vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, struct platform_state* pstate) {
  /// TODO: create custom allocator -- for now use default
  context.allocator = 0; 
  // Setup the vulkan instance
  VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.apiVersion = VK_API_VERSION_1_2;
  app_info.pApplicationName = application_name;
  app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
  app_info.pEngineName = "Pegasus Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1,0,0);

  VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = 0;
  create_info.ppEnabledExtensionNames = 0;
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = 0;

  VkResult result = vkCreateInstance(&create_info, context.allocator, &context.instance);
  if (result != VK_SUCCESS) {
    P_ERROR("ckCreateInstance failed with result: %u", result);
    return FALSE;
  }

  P_INFO("Vulkan Renderer Initialized Successfully");
  return TRUE;
}

void 
vulkan_renderer_backend_shutdown(renderer_backend* backend) {
  return;
}

void 
vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {

}

b8 
vulkan_renderer_backend_begin_frame(renderer_backend* backdend, f32 delta_time) {
  return TRUE;
}

b8 
vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time) {
  return TRUE;
}
