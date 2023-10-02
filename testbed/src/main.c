#include <test.h>
#include <core/logger.h>
#include <core/assert.h>

int 
main(void) {
  P_FATAL("test message: %f", 3.14f);
  P_ERROR("test message: %f", 3.14f);
  P_WARN("test message: %f", 3.14f);
  P_INFO("test message: %f", 3.14f);
  P_DEBUG("test message: %f", 3.14f);
  P_TRACE("test message: %f", 3.14f);

  P_ASSERT(1 == 0);
  

  return 0;
}