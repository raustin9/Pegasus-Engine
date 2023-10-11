#include "vulkan_swapchain.h"
#include "core/logger.h"
#include "core/pmemory.h"
#include "vulkan_device.h"
#include "vulkan_image.h"

void create(vulkan_context* context, u32 width, u32 height, vulkan_swapchain* swapchain);
void destroy(vulkan_context* context, vulkan_swapchain* swapchain);

void
vulkan_swapchain_create(vulkan_context* context, u32 width, u32 height, vulkan_swapchain* out_swapchain) {
  // Create a new one
  create(context, width, height, out_swapchain);
}
void
vulkan_swapchain_recreate(vulkan_context* context, u32 width, u32 height, vulkan_swapchain* swapchain) {
  // Destroy the old swapchain and create a new one
  destroy(context, swapchain);
  create(context, width, height, swapchain);
}

void
vulkan_swapchain_destroy(vulkan_context* context, vulkan_swapchain* swapchain) {
  destroy(context, swapchain);
}

b8 
vulkan_swapchain_acquire_next_image_index(
  vulkan_context* context,
  vulkan_swapchain* swapchain,
  u64 timeout_ms,
  VkSemaphore imgage_available_semaphore,
  VkFence fence,
  u32* out_image_index) {

  // Get the index of the next image to be rendered
  VkResult result = vkAcquireNextImageKHR(
    context->device.logical_device,
    swapchain->handle,
    timeout_ms,
    imgage_available_semaphore,
    fence,
    out_image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
    return FALSE;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    P_FATAL("Failed to acquire swapchain image");
    return FALSE;
  }

  return TRUE;
}

// Performs the presentation of a rendered-to image from the swapchain
void vulkan_swapchain_present(
  vulkan_context* context,
  vulkan_swapchain* swapchain,
  VkQueue graphics_queue,
  VkQueue present_queue,
  VkSemaphore render_complete_semaphore,
  u32 present_image_index) {

  VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_complete_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain->handle;
  present_info.pImageIndices = &present_image_index;
  present_info.pResults = NULL;
  // present_info.pNext = NULL;

  VkResult result = vkQueuePresentKHR(present_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // Swapchain is out of date, suboptimal or a framebuffer resize has occured. Trigger swapchain recreation
    vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
  } else if (result != VK_SUCCESS) {
    P_FATAL("Failed to present swapchain iamge");
  }
}

void 
create(vulkan_context* context, u32 width, u32 height, vulkan_swapchain* swapchain) {
  VkExtent2D swapchain_extent = {width, height};
  swapchain->max_frames_in_flight = 2; // triple buffering if possible | 2 frames being rendered to as we draw and present a third

  b8 found = FALSE;
  for (u32 i = 0; i < context->device.swapchain_support.format_count; i++) {
    VkSurfaceFormatKHR format = context->device.swapchain_support.formats[i];
    // Preferred formats
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        swapchain->image_format = format;
        found = TRUE;
        break;
      }
  }

  if (!found) {
    swapchain->image_format = context->device.swapchain_support.formats[0];
  }

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // this mode has to exist according to Vulkan spec
  for (u32 i = 0; i < context->device.swapchain_support.present_mode_count; i++) {
    VkPresentModeKHR mode = context->device.swapchain_support.present_modes[i];
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      // Mailbox will use the most current image/frame that has been rendered rather than
      // enforcing an order of age. This way the frame being rendered is always the most
      // current
      present_mode = mode;
      break;
    } // we can use IMMEDIATE rather than mailbox which is faster
      // but it is not vsync so there is a chance of screen tearing
  }

  // Requery swapchain support to see what it is currently capable of rendering to
  vulkan_device_query_swapchain_support(
    context->device.physical_device,
    context->surface,
    &context->device.swapchain_support
  );

  // If device does not support the current extent, overwrite it with something that is
  if (context->device.swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
    swapchain_extent = context->device.swapchain_support.capabilities.currentExtent;
  }

  VkExtent2D min = context->device.swapchain_support.capabilities.minImageExtent;
  VkExtent2D max = context->device.swapchain_support.capabilities.maxImageExtent;
  swapchain_extent.width = PCLAMP(swapchain_extent.width, min.width, max.width);
  swapchain_extent.height = PCLAMP(swapchain_extent.height, min.height, max.height);

  u32 image_count = context->device.swapchain_support.capabilities.minImageCount+1; // almost always be 2
  if (context->device.swapchain_support.capabilities.maxImageCount > 0 && image_count > context->device.swapchain_support.capabilities.maxImageCount) {
    image_count = context->device.swapchain_support.capabilities.maxImageCount;
  }

  // Set up the swapchain create info
  VkSwapchainCreateInfoKHR swapchain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchain_info.surface = context->surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = swapchain->image_format.format;
  swapchain_info.imageColorSpace = swapchain->image_format.colorSpace;
  swapchain_info.imageExtent = swapchain_extent;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // use these images to render to the 'color buffer'

  // Setup the queue family indices
  if (context->device.graphics_queue_index != context->device.present_queue_index) {
    u32 queue_family_indices[] = {
      (u32)context->device.graphics_queue_index,
      (u32)context->device.present_queue_index
    };
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = 0;
  }

  swapchain_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = present_mode;
  swapchain_info.clipped = VK_TRUE;
  // swapchain_info.oldSwapchain = 0;
  swapchain_info.oldSwapchain = NULL; // this will have to change as we recreate the swapchain

  VK_CHECK(vkCreateSwapchainKHR(
    context->device.logical_device,
    &swapchain_info,
    context->allocator,
    &swapchain->handle
  ));

  context->current_frame = 0;

  swapchain->image_count = 0;
  VK_CHECK(vkGetSwapchainImagesKHR(
    context->device.logical_device,
    swapchain->handle,
    &swapchain->image_count,
    0
  ));
  if (!swapchain->images) {
    swapchain->images = (VkImage*)pallocate(sizeof(VkImage) * swapchain->image_count, MEMORY_TAG_RENDERER);
  }
  if (!swapchain->views) {
    swapchain->views = (VkImageView*)pallocate(sizeof(VkImageView) * swapchain->image_count, MEMORY_TAG_RENDERER);
  }

  // Obtain the images from the swapchain
  VK_CHECK(vkGetSwapchainImagesKHR(
    context->device.logical_device,
    swapchain->handle,
    &swapchain->image_count,
    swapchain->images
  ));

  // Create the image views
  for (u32 i = 0; i < swapchain->image_count; i++) {
    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = swapchain->images[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = swapchain->image_format.format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(
      context->device.logical_device,
      &view_info,
      context->allocator,
      &swapchain->views[i]
    ));
  }

  // Create resource for the depth buffer
  if (!vulkan_device_detect_depth_format(&context->device)) {
    context->device.depth_format = VK_FORMAT_UNDEFINED;
    P_FATAL("Failed to find a supported format");
  }

  // Create the depth buffer
  vulkan_image_create(
    context,
    VK_IMAGE_TYPE_2D,
    swapchain_extent.width,
    swapchain_extent.height,
    context->device.depth_format,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    TRUE,
    VK_IMAGE_ASPECT_DEPTH_BIT,
    &swapchain->depth_attachment
  );

  P_INFO("Swapchain created successfully.");
}

void 
destroy(vulkan_context* context, vulkan_swapchain* swapchain) {
  vulkan_image_destroy(context, &swapchain->depth_attachment);

  // Only destroy the views not images since those are owned by the swapchain
  // and are destroyed when it is
  for (u32 i = 0; i < swapchain->image_count; i++) {
    vkDestroyImageView(context->device.logical_device, swapchain->views[i], context->allocator);
  }

  vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator);
}