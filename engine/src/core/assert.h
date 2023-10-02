#pragma once

#include "defines.h"

// Disable assertions by commenting this out
#define P_ASSERTIONS_ENABLED

#ifdef P_ASSERTIONS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

P_API void report_assertion_failure(const char* expression, const char* messae, const char* file, i32 line);

#define P_ASSERT(expr)                                       \
{                                                            \
  if (expr) {                                                \
  } else {                                                   \
    report_assertion_failure(#expr, "", __FILE__, __LINE__); \
  }                                                          \
}                                                            

#define P_ASSERT_MSG(expr, message)                                 \
{                                                                   \
  if (expr) {                                                       \
  } else {                                                          \
      report_assertion_failure(#expr, message, __FILE__, __LINE__); \
      debugBreak();                                                 \
  }                                                                 \
}                                                                   

// Gets compiled to nothing when in release build
#ifdef _DEBUG
#define P_ASSERT_DEBUG(expr)                                        \
{                                                                   \
  if (expr) {                                                       \
  } else {                                                          \
      report_assertion_failure(#expr, "", __FILE__, __LINE__);      \
      debugBreak();                                                 \
  }                                                                 \
}                                                                   
#else // _DEBUG
#define P_ASSERT_DEBUG(expr) // does nothing
#endif // _DEBUG

#else // P_ASSERTION_ENABLED
#define P_ASSERT(expr)
#define P_ASSERT_MSG(expr, message)
#define P_ASSERT_DEBUG(expr)
#endif // P_ASSERTION_ENABLED