#include "renderer/vulkan/vulkan_backend.h"
#include "vulkan_types.inl"

#include "core/logger.h"
#include "core/pstring.h"

#include "containers/darray.h"
#include "vulkan_platform.h"
// #include "platform/platform.h"
#include <vulkan/vulkan_core.h>

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

    // Create the vulkan instance
    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    // Required Extensions
    const char** required_extensions = darray_create(const char*);
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME); // generic surface extension
    platform_get_required_extension_names(&required_extensions); // TODO: implement this function
                                                                 
#if defined(_DEBUG) 
    // if we are in DEBUG mode, load this extra extension
    darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    P_DEBUG("Required Extensions:");
    u32 length = darray_length(required_extensions);
    for (u32 i = 0; i < length; i++) {
        P_DEBUG(required_extensions[i]);
    }
#endif

    create_info.enabledExtensionCount = darray_length(required_extensions);
    create_info.ppEnabledExtensionNames = required_extensions;

    // Validation Layers
    const char** required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;

#if defined(_DEBUG)
    P_INFO("Validation layers enabled. Enumerating...");

    // The list of validation layers required
    required_validation_layer_names = darray_create(const char*);
    darray_push(required_validation_layer_names, &"VK_LAYER_KHRONOS_validation");
    required_validation_layer_count = darray_length(required_validation_layer_names);

    // Obtain a list of available validation layers
    u32 available_layer_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    VkLayerProperties* available_layers = darray_reserve(VkLayerProperties, available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

    // Verify that all required layers are available
    for (u32 i = 0; i < required_validation_layer_count; i++) {
        P_INFO("Searching for layer: %s...", required_validation_layer_names[i]);
        b8 found = FALSE;
        for (u32 j = 0; j < available_layer_count; j++) {
            if (strings_equal(required_validation_layer_names[i], available_layers[j].layerName)) {
                found = TRUE;
                P_INFO("Found.");
                break;
            }
        }

        if (!found) {
            P_FATAL("Required validation layer is missing: %s", required_validation_layer_names[i]);
            return FALSE;
        }
    }

    P_INFO("All required validation layers are present");
#endif /* _DEBUG */

    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = required_validation_layer_names;

    VK_CHECK(vkCreateInstance(&create_info, context.allocator, &context.instance));
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
