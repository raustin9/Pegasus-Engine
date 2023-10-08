#pragma once

#include "defines.h"

typedef struct clock {
  f64 start_time;
  f64 elapsed;
} clock;

// CLOCK INTERFACE

// Has no effect on non-started clocks
void clock_update(clock* clock);

// Starts the provided clock. Resets elapsed time
void clock_start(clock* clock);

// Stops the input clock. Does not reset the elapsed time
void clock_stop(clock* clock);