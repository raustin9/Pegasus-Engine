#pragma once

#include "defines.h"

// Get the required extensions for each platform
// @param ext_darray - an array of char arrays 
// This appends all the required extension names for 
// this platform onto the ext_darray
void platform_get_required_extension_names(const char*** ext_darray);
