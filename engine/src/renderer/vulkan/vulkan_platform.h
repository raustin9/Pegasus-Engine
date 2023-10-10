#pragma once

#include "defines.h"
struct platform_state;
struct vulkan_context;

// Create the surface vulkan will render to
b8 platform_create_vulkan_surface(
        struct platform_state* pstate,
        struct vulkan_context* context);

// Get the required extensions for each platform
// @param ext_darray - an array of char arrays 
// This appends all the required extension names for 
// this platform onto the ext_darray
void platform_get_required_extension_names(const char*** ext_darray);
