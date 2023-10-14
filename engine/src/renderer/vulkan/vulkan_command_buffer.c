#include "vulkan_command_buffer.h"
#include "core/pmemory.h"

#include "vulkan_types.inl"

void vulkan_command_buffer_allocate(
  vulkan_context* context,
  VkCommandPool pool,
  b8 is_primary,
  vulkan_command_buffer* out_command_buffer
) {
  VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocate_info.commandPool = pool;
  allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  allocate_info.commandBufferCount = 1;
  allocate_info.pNext = NULL;

  out_command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
  VK_CHECK(vkAllocateCommandBuffers(
    context->device.logical_device,
    &allocate_info,
    &out_command_buffer->handle // pass the address because it is only one buffer. If there was multiple we would pass the array because it is a poitner
  ));
}

void vulkan_command_buffer_free(
  vulkan_context* context,
  VkCommandPool pool,
  vulkan_command_buffer* command_buffer
) {
  vkFreeCommandBuffers(
    context->device.logical_device,
    pool,
    1,
    &command_buffer->handle
  );

  command_buffer->handle = NULL;
  command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
}

void vulkan_command_buffer_begin(
  vulkan_command_buffer* command_buffer,
  b8 is_single_use,
  b8 is_renderpass_continue,
  b8 is_simultaneous_use
) {
  VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = 0;
  if (is_single_use) {
    // One use then discarded
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  }
  if (is_renderpass_continue) {
    // A secondary buffer inside of a renderpass
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  }
  if (is_simultaneous_use) {
    // Buffer can be resubmitted while it is PENDING or "not submitted"
    // can be recorded to multiple command buffers
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  }

  VK_CHECK(vkBeginCommandBuffer(
    command_buffer->handle,
    &begin_info
  ));

  command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}

void vulkan_command_buffer_end(vulkan_command_buffer* command_buffer) {
  VK_CHECK(vkEndCommandBuffer(
    command_buffer->handle
  ));
  command_buffer->state = COMMAND_BUFFER_STATE_RECORDING_ENDED;
  // TODO: should check to see if we are in state where ending is valid
  // but for now just allow application to crash
}

void vulkan_command_buffer_update_submitted(vulkan_command_buffer* command_buffer) {
  command_buffer->state = COMMAND_BUFFER_STATE_SUBMITTED;
  // TODO: potentially add checking
}

void vulkan_command_buffer_reset(vulkan_command_buffer* command_buffer) {
  command_buffer->state = COMMAND_BUFFER_STATE_READY;
  // TODO: potentially add checking
}

void vulkan_command_buffer_allocate_and_begin_single_use(
  vulkan_context* context,
  VkCommandPool pool,
  vulkan_command_buffer* out_command_buffer
) {
  vulkan_command_buffer_allocate(context, pool, TRUE, out_command_buffer);
  vulkan_command_buffer_begin(out_command_buffer, TRUE, FALSE, FALSE); // make assumptions about these boolean values
}

void vulkan_command_buffer_end_single_use(
  vulkan_context* context,
  VkCommandPool pool,
  vulkan_command_buffer* command_buffer,
  VkQueue queue
) {
  // End command buffer
  vulkan_command_buffer_end(command_buffer);

  // Submit the queue
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer->handle;
  VK_CHECK(vkQueueSubmit(
    queue,
    1,
    &submit_info,
    0
  ));

  // Wait for it ot finish
  VK_CHECK(vkQueueWaitIdle(queue));

  // Free the command buffer
  vulkan_command_buffer_free(context, pool, command_buffer);
}