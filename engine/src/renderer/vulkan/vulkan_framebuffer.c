#include "vulkan_framebuffer.h"

#include "core/pmemory.h"

void vulkan_framebuffer_create(
  vulkan_context* context,
  vulkan_renderpass* renderpass,
  u32 width,
  u32 height,
  u32 attachment_count,
  VkImageView* attachments,
  vulkan_framebuffer* out_framebuffer
) {
  // Get a copy of the attachments so we don't have to worry about lifetimes
  out_framebuffer->attachments = pallocate(sizeof(VkImageView) * attachment_count, MEMORY_TAG_RENDERER);
  for (u32 i = 0; i < attachment_count; i++) {
    out_framebuffer->attachments[i] = attachments[i];
  }

  out_framebuffer->renderpass = renderpass;
  out_framebuffer->attachment_count = attachment_count;

  VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  create_info.renderPass = renderpass->handle;
  create_info.attachmentCount = attachment_count;
  create_info.pAttachments = out_framebuffer->attachments;
  create_info.width = width;
  create_info.height = height;
  create_info.layers = 1;

  VK_CHECK(vkCreateFramebuffer(
    context->device.logical_device,
    &create_info,
    context->allocator,
    &out_framebuffer->handle
  ));
}

void vulkan_framebuffer_destroy(
  vulkan_context* context,
  vulkan_framebuffer* framebuffer
) {
  vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);
  if (framebuffer->attachments) {
    pfree(framebuffer->attachments, sizeof(VkImageView) * framebuffer->attachment_count, MEMORY_TAG_RENDERER);
    framebuffer->attachments = NULL;
  }

  framebuffer->handle = NULL;
  framebuffer->attachment_count = 0;
  framebuffer->renderpass = NULL;
}