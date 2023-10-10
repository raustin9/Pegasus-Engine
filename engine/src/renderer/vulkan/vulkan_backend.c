#include "renderer/vulkan/vulkan_backend.h"
#include "vulkan_types.inl"
#include "vulkan_device.h"
#include "assert.h"

#include "core/logger.h"
#include "core/pstring.h"

#include "containers/darray.h"
#include "vulkan_platform.h"
#include "platform/platform.h"
#include <vulkan/vk_platform.h>

static vulkan_context context;

// Foward declare our debug message callback function
VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

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
    P_INFO("Vulkan instance created");

    // Create the debugger
#if defined(_DEBUG)
    P_DEBUG("Creating vulkan debugger...");
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                       // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                       // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BUT_EXT
                       ;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;
    debug_create_info.pUserData = NULL;

    PFN_vkCreateDebugUtilsMessengerEXT func = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
    P_ASSERT_MSG(func, "Failed to create debug messenger");
    VK_CHECK(func(context.instance, &debug_create_info, context.allocator, &context.debug_messenger));
    P_DEBUG("Vulkan debugger created.");
#endif /* _DEBUG */
  
    // Create surface
    P_DEBUG("Creating Vulkan surface...");
    if (!platform_create_vulkan_surface(pstate, &context)) {
        P_ERROR("Unable to create Vulkan surface");
        return FALSE;
    }
    P_DEBUG("Vulkan surface created.");


    // Create device
    if (!vulkan_device_create(&context)) {
        P_ERROR("Unable to create device");
        return FALSE;
    }

    P_INFO("Vulkan Renderer Initialized Successfully");
    return TRUE;
}

void 
vulkan_renderer_backend_shutdown(renderer_backend* backend) {
    // Destroy resources in reverse order from creation
    P_DEBUG("Destroying Vulkan debugger");
    if (context.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = 
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context.instance, context.debug_messenger, context.allocator);
    }

    vulkan_device_destroy(&context);
    
    P_DEBUG("Destroying Vulkan instance...");
    vkDestroyInstance(context.instance, context.allocator);

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

// Callback function for our debug messenger
// This takes the severity of the message and forwards
// the debug message to the appropriate logger
VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            P_ERROR(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            P_WARN(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            P_INFO(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            P_TRACE(callback_data->pMessage);
            break;
    }

    return VK_FALSE; // always return false according to the spec
}
