#pragma once

#include "defines.h"

P_API char* string_duplicate(const char* string);
P_API u64 string_length(const char* str);

// Case-sensitive comparison of 2 strings. Return TRUE if the same FALSE otherwise
P_API b8 strings_equal(const char* s1, const char* s2);
