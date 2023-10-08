#include "renderer_frontend.h"
#include "renderer_backend.h"

#include "core/logger.h"
#include "core/pmemory.h"

// Backend render context
static renderer_backend* backend = 0;

b8
renderer_initialize(const char* applicaation_name, struct platform_state* pstate) {
  backend = pallocate(sizeof(renderer_backend), MEMORY_TAG_RENDERER);
  // P_DEBUG("Backend address: %x size: %lu", backend, sizeof(renderer_backend));

  /// TODO: make this configurable
  renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, pstate, backend);
  backend->frame_number = 0;

  if (!backend->initialize(backend, applicaation_name, pstate)) {
    P_FATAL("Renderer backend failed to initialize");
    return FALSE;
  }
  // P_DEBUG("PAST render_backend_create");


  return TRUE;
}

void
renderer_shutdown() {
  backend->shutdown(backend);
  pfree(backend, sizeof(renderer_backend), MEMORY_TAG_RENDERER);
}

b8
renderer_begin_frame(f32 delta_time) {
  return backend->begin_frame(backend, delta_time);
}

b8
renderer_end_frame(f32 delta_time) {
  b8 result = backend->end_frame(backend, delta_time);
  backend->frame_number++;
  return result;
}

b8
renderer_draw_frame(render_packet* packet) {
  // If the begin frame returned successfully, mid-frame operations may continue
  if (renderer_begin_frame(packet->delta_time)) {
    // End the frame. If this failed, it is likely unrecoverable
    b8 result = renderer_end_frame(packet->delta_time);

    if (!result) {
      P_ERROR("renderer_end_frame returned unsuccessfully");
      return FALSE;
    }
  }

  return TRUE;
}