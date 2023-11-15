#include "vulkan_backend.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_command_buffer.h"
#include "vulkan_framebuffer.h"
#include "vulkan_fence.h"
#include "vulkan_utils.h"

#include "assert.h"

#include "core/logger.h"
#include "core/pstring.h"
#include "core/pmemory.h"
#include "core/application.h"

#include "containers/darray.h"
#include "vulkan_platform.h"
#include "platform/platform.h"
#include <vulkan/vk_platform.h>

static vulkan_context context;
static u32 cached_framebuffer_width = 0;
static u32 cached_framebuffer_height = 0;

// Foward declare our debug message callback function
VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

i32 find_memory_index(u32 type_filter, u32 property_flags);

void create_command_buffers(renderer_backend* backend); // does the initial creation of the command buffers
void regenerate_framebuffers(renderer_backend* backend, vulkan_swapchain* swapchain, vulkan_renderpass* renderpass); // any time we change certain factors of the window, we have to recreate the swapchain and command buffers
b8 recreate_swapchain(renderer_backend* backend);

b8 
vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, struct platform_state* pstate) {
    // Function pointers
    context.find_memory_index = find_memory_index;

    /// TODO: create custom allocator -- for now use default
    context.allocator = NULL;

    application_get_framebuffer_size(&cached_framebuffer_width, &cached_framebuffer_height);
    context.framebuffer_width = (cached_framebuffer_width != 0) ? cached_framebuffer_width : 800;
    context.framebuffer_height = (cached_framebuffer_height != 0) ? cached_framebuffer_height : 600;
    context.framebuffer_size_generation = 0;
    context.framebuffer_size_last_generation = 0;
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;

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

#if defined(_DEBUG) // if we are debugging, find the validation layer names we want/can use
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
    // This is done by the surface, so that is why we use the platform to do this
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

    // Create swapchain
    vulkan_swapchain_create(
      &context,
      context.framebuffer_width,
      context.framebuffer_height,
      &context.swapchain
    );

    vulkan_renderpass_create(
        &context,
        &context.main_renderpass,
        0, 0, context.framebuffer_width, context.framebuffer_height,
        0.0f, 0.0f, 0.2f, 1.0f,
        1.0f,
        0
    );

    // Swapchain framebuffers
    context.swapchain.framebuffers = darray_reserve(vulkan_framebuffer, context.swapchain.image_count);
    regenerate_framebuffers(backend, &context.swapchain, &context.main_renderpass);

    // Create command buffers
    create_command_buffers(backend);

    // Create sync objects
    context.image_availale_semaphores = darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);
    context.queue_complete_semaphores = darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);
    context.in_flight_fences = darray_reserve(vulkan_fence, context.swapchain.max_frames_in_flight);

    for (u32 i = 0; i < context.swapchain.max_frames_in_flight; i++) {
        VkSemaphoreCreateInfo semaphore_create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info, context.allocator, &context.image_availale_semaphores[i]);
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info, context.allocator, &context.queue_complete_semaphores[i]);

        // Create the fence in a simplified state indicating that the first frame has already been "rendered"
        // This prevents the application from waiting indefinitely for the first frame to render since
        // it cannot be rendered until a frame is "rendered before it"
        vulkan_fence_create(&context, TRUE, &context.in_flight_fences[i]);
    }

    // In flight fences should not yet exist, so clear the list. These are stored in
    // pointers because the initial state should be 0, and will be 0 when not in use.
    // Actual fences are not owned by this list
    context.images_in_flight = darray_reserve(vulkan_fence, context.swapchain.image_count);
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        context.images_in_flight[i] = 0;
    }


    P_INFO("Vulkan Renderer Initialized Successfully");
    return TRUE;
}

void 
vulkan_renderer_backend_shutdown(renderer_backend* backend) {
    vkDeviceWaitIdle(context.device.logical_device);
    // Destroy resources in reverse order from creation

    // Sync objects
    for (u8 i = 0; i < context.swapchain.max_frames_in_flight; i++) {
        if (context.image_availale_semaphores[i]) {
            vkDestroySemaphore(
                context.device.logical_device,
                context.image_availale_semaphores[i],
                context.allocator
            );
        }
        if (context.queue_complete_semaphores[i]) {
            vkDestroySemaphore(
                context.device.logical_device,
                context.queue_complete_semaphores[i],
                context.allocator
            );
        }
        vulkan_fence_destroy(&context, &context.in_flight_fences[i]);
    }
    darray_destroy(context.image_availale_semaphores);
    context.image_availale_semaphores = NULL;

    darray_destroy(context.queue_complete_semaphores);
    context.queue_complete_semaphores = NULL;

    darray_destroy(context.in_flight_fences);
    context.in_flight_fences = NULL;

    darray_destroy(context.images_in_flight);
    context.images_in_flight = NULL;


    // Command buffers
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        if (context.graphics_command_buffers[i].handle) {
            vulkan_command_buffer_free(
                &context,
                context.device.graphics_command_pool,
                &context.graphics_command_buffers[i]
            );
            context.graphics_command_buffers[i].handle = NULL;
        }
    }
    darray_destroy(context.graphics_command_buffers);
    context.graphics_command_buffers = NULL;

    // Framebuffers
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vulkan_framebuffer_destroy(&context, &context.swapchain.framebuffers[i]);
    }

    // Renderpass
    P_DEBUG("Destroying renderpass...");
    vulkan_renderpass_destroy(&context, &context.main_renderpass);

    // Swapchain
    vulkan_swapchain_destroy(&context, &context.swapchain);

    // Device
    P_DEBUG("Destroying vulkan device...");
    vulkan_device_destroy(&context);

    // Surface
    P_DEBUG("Destroying vulkan surface...");
    if (context.surface) {
      vkDestroySurfaceKHR(
        context.instance,
        context.surface,
        context.allocator
      );
      context.surface = NULL;
    }
    
    // Debugger
    P_DEBUG("Destroying Vulkan debugger...");
    if (context.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = 
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context.instance, context.debug_messenger, context.allocator);
    }
    
    // Instance
    P_DEBUG("Destroying Vulkan instance...");
    vkDestroyInstance(context.instance, context.allocator);

    return;
}

void 
vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {
    cached_framebuffer_width = width;
    cached_framebuffer_height = height;
    context.framebuffer_size_generation++;

    P_INFO("Vulkan renderer backend->resized: w/h/gen: %i/%i/%llu", width, height, context.framebuffer_size_generation);
}

b8 
vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
    vulkan_device* device = &context.device;

    // Check if we are resizing the swapchain
    if (context.recreating_swapchain) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            P_ERROR("vulkan_renderer_backend_begin_frame vkDeviceWaitIdle (1) failed: '%s'", vulkan_result_string(result, TRUE));
            return FALSE;
        }
        P_INFO("Recreating swapchain, booting...");
        return FALSE;
    }

    // Check if the framebuffer has been resized
    // If so, a new swapchain must be recreated
    if (context.framebuffer_size_generation != context.framebuffer_size_last_generation) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            P_ERROR("vulkan_renderer_backend_begin_frame vkDeviceWaitIdle (2) failed: '%s'", vulkan_result_string(result, TRUE));
            return FALSE;   
        }

        // If the swapchain recreation failed (for example if the window was minimized)
        // boot out before unsetting the flag
        if (!recreate_swapchain(backend)) {
            return FALSE;
        }

        P_INFO("Resized, booting...");
        return FALSE;
    }

    // Wait for the execution of the current frame to complete
    // The fence being free will allow this to move on
    if (!vulkan_fence_wait(
        &context,
        &context.in_flight_fences[context.current_frame],
        UINT64_MAX
    )) {
        P_WARN("In-flight fence wait failure!");
        return FALSE;
    }

    // Acquire the swapchain next image. Pass along the semaphore that should
    // be signaled when this completes. This same semaphore will be waited
    // on by teh queue submission to ensure this image is available
    if (!vulkan_swapchain_acquire_next_image_index(
        &context,
        &context.swapchain,
        UINT64_MAX,
        context.image_availale_semaphores[context.current_frame],
        0,
        &context.image_index
    )) {
        return FALSE;
    }

    // Begin recording command buffers
    vulkan_command_buffer* command_buffer = &context.graphics_command_buffers[context.image_index];
    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin(command_buffer, FALSE, FALSE, FALSE);

    // Dynamic states
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)context.framebuffer_height;
    viewport.width = (f32)context.framebuffer_width;
    viewport.height = -(f32)context.framebuffer_height; // make this negative to make it like OpenGL
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = context.framebuffer_width;
    scissor.extent.height = context.framebuffer_height;

    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);

    context.main_renderpass.w = context.framebuffer_width;
    context.main_renderpass.h = context.framebuffer_height;

    // Begin the render pass
    vulkan_renderpass_begin(
        command_buffer,
        &context.main_renderpass,
        context.swapchain.framebuffers[context.image_index].handle
    );
    
    return TRUE;
}

b8 
vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time) {
    vulkan_command_buffer* command_buffer = &context.graphics_command_buffers[context.image_index];

    // End renderpass
    vulkan_renderpass_end(
        command_buffer,
        &context.main_renderpass
    );

    vulkan_command_buffer_end(command_buffer);

    // Make sure that the previous frame is not using this image
    if (context.images_in_flight[context.image_index] != VK_NULL_HANDLE) {
        vulkan_fence_wait(
            &context,
            context.images_in_flight[context.image_index],
            UINT64_MAX);
    }    

    // Mark the image fence as in use by this frame
    context.images_in_flight[context.image_index] = &context.in_flight_fences[context.current_frame];

    // Reset the fence for use on the next frame
    vulkan_fence_reset(&context, &context.in_flight_fences[context.current_frame]);

    // Begin queue submission step
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &context.queue_complete_semaphores[context.current_frame];

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &context.image_availale_semaphores[context.current_frame];

    // Each semaphore waits on teh corresponding pipeline stage to complete
    VkPipelineStageFlags flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = flags;

    // Submit all the commands to be executed
    VkResult result = vkQueueSubmit(
        context.device.graphics_queue,
        1,
        &submit_info,
        context.in_flight_fences[context.current_frame].handle
    );

    if (result != VK_SUCCESS) {
        P_ERROR("vkQueueSubmit failed with result: '%s'", vulkan_result_string(result, TRUE));
        return FALSE;
    }

    vulkan_command_buffer_update_submitted(command_buffer);
    // End queue submission

    // Presentation
    vulkan_swapchain_present(
        &context,
        &context.swapchain,
        context.device.graphics_queue,
        context.device.present_queue,
        context.queue_complete_semaphores[context.current_frame],
        context.image_index
    );

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

i32 
find_memory_index(u32 type_filter, u32 property_flags) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(context.device.physical_device, &mem_properties);

    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
      // Check each memory type t osee if its bit is set t 1
      if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags) == property_flags) {
        return i;
      }
    }

    P_WARN("Unable to find suitable memory type");
    return -1;
}

void
create_command_buffers(renderer_backend* backend) {
    // Create a command buffer for each of our swapchain images
    // While on image is presenting, we need to draw to the others
    // in the swapchain. We have to use a different command buffer
    // for each swapchain image
    if (!context.graphics_command_buffers) {
        context.graphics_command_buffers = darray_reserve(vulkan_command_buffer, context.swapchain.image_count);
        for (u32 i = 0; i < context.swapchain.image_count; i++) {
            pzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
        }
    }

    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        if (context.graphics_command_buffers[i].handle) {
            vulkan_command_buffer_free(
                &context,
                context.device.graphics_command_pool,
                &context.graphics_command_buffers[i]
            );

        }
        pzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
        vulkan_command_buffer_allocate(
            &context,
            context.device.graphics_command_pool,
            TRUE,
            &context.graphics_command_buffers[i]
        );
    }

    P_INFO("Graphics command buffers created");
}

void regenerate_framebuffers(
    renderer_backend* backend, 
    vulkan_swapchain* swapchain, 
    vulkan_renderpass* renderpass
) {
    // Need separate framebuffer for each image in the swapchain
    for (u32 i = 0; i < swapchain->image_count; i++) {
        u32 attachment_count = 2;
        VkImageView attachments[] = {
            swapchain->views[i],
            swapchain->depth_attachment.view
        };

        // Create the framebuffer for the swapchain image
        vulkan_framebuffer_create(
            &context,
            renderpass,
            context.framebuffer_width,
            context.framebuffer_height,
            attachment_count,
            attachments,
            &context.swapchain.framebuffers[i]
        );
    }
}

b8
recreate_swapchain(renderer_backend* backend) {
    if (context.recreating_swapchain) {
        P_DEBUG("recreate_swapchain called when already recreating. Booting...");
        return FALSE;
    }

    if (context.framebuffer_height == 0 || context.framebuffer_width == 0) {
        P_DEBUG("recreate_swapchain called when window is < 1 in a dimension. Booting...");
        return FALSE;
    }

    // Mark as recreating if hte dimensions are valid
    context.recreating_swapchain = TRUE;

    // Wait for any operations to complete
    vkDeviceWaitIdle(context.device.logical_device);

    // Clear in case
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        context.images_in_flight[i] = NULL;
    }

    // Requery support
    vulkan_device_query_swapchain_support(
        context.device.physical_device,
        context.surface,
        &context.device.swapchain_support
    );
    vulkan_device_detect_depth_format(&context.device);

    // recreate the swapchain
    vulkan_swapchain_recreate(
        &context,
        cached_framebuffer_width,
        cached_framebuffer_height,
        &context.swapchain
    );

    // Sync the framebuffer size with the cached sizes
    context.framebuffer_height = cached_framebuffer_height;
    context.framebuffer_width = cached_framebuffer_width;
    context.main_renderpass.w = context.framebuffer_width;
    context.main_renderpass.h = context.framebuffer_height;
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;

    // update framebuffer size generation
    context.framebuffer_size_last_generation = context.framebuffer_size_generation;

    // cleanup swapchain
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);
    }

    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vulkan_framebuffer_destroy(&context, &context.swapchain.framebuffers[i]);
    }

    context.main_renderpass.x = 0;
    context.main_renderpass.y = 0;
    context.main_renderpass.w = context.framebuffer_width;
    context.main_renderpass.h = context.framebuffer_height;

    regenerate_framebuffers(backend, &context.swapchain, &context.main_renderpass);

    create_command_buffers(backend);

    // clear the recreating flag
    context.recreating_swapchain = FALSE;
    return TRUE;
}