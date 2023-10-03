#pragma once

#include "defines.h"

// Maintain platform state in a decoupled way
typedef struct platform_state {
  void* internal_state;
} platform_state;


P_API b8 platform_startup(
  platform_state* pstate,
  const char* application_name,
  i32 x,
  i32 y,
  i32 width,
  i32 height
);

P_API void platform_shutdown(platform_state *pstate);

P_API b8 platform_pump_messages(platform_state* pstate);

// Deal with platform memory
P_API void* platform_allocate(u64 size, b8 aligned);
P_API void platform_free(void* block, b8 aligned);
void* platform_zero_memory(void* dest, u64 size);
void* platform_copy_memory(void* dest, const void* source, u64 size);
void* platform_set_memory(void* dest, i32 value, u64 size);

// Write colored text to the console
void platform_console_write(const char* message, u8 color);
void platform_console_write_error(const char* message, u8 color);

// Get the time
f64 platform_get_absolute_time();

// Sleep on the thread for the provided ms. This blocks the main thread.
// This should only be used for giving time back to the OS for unused update power
// Therefore it is not exported
void platform_sleep(u64 ms);
