#include <test.h>
#include <core/logger.h>
#include <core/assert.h>
#include <platform/platform.h>

int 
main(void) {
  P_FATAL("test message: %f", 3.14f);
  P_ERROR("test message: %f", 3.14f);
  P_WARN("test message: %f", 3.14f);
  P_INFO("test message: %f", 3.14f);
  P_DEBUG("test message: %f", 3.14f);
  P_TRACE("test message: %f", 3.14f);

  platform_state state;

  if (platform_startup(&state, "Pegasus Engine Testbed", 100, 100, 1280, 720)) {
    while (TRUE) {
      platform_pump_messages(&state);
    }
  }

  platform_shutdown(&state);
  

  return 0;
}