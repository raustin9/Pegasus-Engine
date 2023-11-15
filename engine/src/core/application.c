#include "application.h"
#include "game_types.h"

#include "platform/platform.h"
#include "core/pmemory.h"
#include "core/event.h"
#include "core/input.h"
#include "logger.h"
#include "clock.h"

#include "renderer/renderer_frontend.h"

#include <string.h>

typedef struct application_state {
    game* game_inst;
    b8 is_running;
    b8 is_suspended;
    platform_state platform;
    i16 width;
    i16 height;
    clock clock;
    f64 last_time;
} application_state;

static b8 initialized = FALSE;
static application_state app_state;

b8 application_on_event(u16 code, void* sender, void* listener_inst, event_context context);
b8 application_on_key(u16 code, void* sender, void* listener_inst, event_context context);
b8 application_on_resize(u16 code, void* sender, void* listener_inst, event_context context);

b8
application_create(game* game_inst) {
    if (initialized) {
        P_ERROR("application_create called more than once");
        return FALSE;
    }

    app_state.game_inst = game_inst;

    // Initialize subsystems
    // initialize_memory();
    initialize_logging();
    input_initialize();

    app_state.is_running = TRUE;
    app_state.is_suspended = FALSE;

    // Initialize event system
    if (!event_initialize()) {
        P_ERROR("Event system could not initialize. Application cannot continue");
        return FALSE;
    }

    // Register for events
    event_register(EVENT_CODE_APPLICATION_QUIT, NULL, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, NULL, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, NULL, application_on_key);
    event_register(EVENT_CODE_RESIZED, NULL, application_on_resize);

    if (!platform_startup(
            &app_state.platform, 
            game_inst->app_config.name, 
            game_inst->app_config.start_pos_x, 
            game_inst->app_config.start_pos_y, 
            game_inst->app_config.start_width, 
            game_inst->app_config.start_height
        )) {
        return FALSE;
    }

    // P_TRACE("GOT HERE");
    // Renderer startup
    if (!renderer_initialize(game_inst->app_config.name, &app_state.platform)) {
        P_FATAL("Unable to initialize renderer. Aborting application");
        return FALSE;
    }

    // Initialize the game
    if (!app_state.game_inst->initialize(app_state.game_inst)) {
        P_FATAL("Game failed to initialize");
        return FALSE;
    }

    app_state.game_inst->on_resize(app_state.game_inst, app_state.width, app_state.height);
    
    initialized = TRUE;

    return TRUE;
}

b8
application_run() {
    clock_start(&app_state.clock);
    clock_update(&app_state.clock);
    app_state.last_time = app_state.clock.elapsed;
    f64 running_time = 0;
    u8 frame_count = 0;
    f64 target_frame_seconds = 1.0F / 60; // 60fps

    char* mem_usage = get_memory_usage_str();
    P_INFO(mem_usage);
    pfree(mem_usage, strlen(mem_usage), MEMORY_TAG_STRING);

    while (app_state.is_running) {
        if (!platform_pump_messages(&app_state.platform)) {
            app_state.is_running = FALSE;
        }

        if (!app_state.is_suspended) {
            clock_update(&app_state.clock);
            f64 current_time = app_state.clock.elapsed;
            f64 delta = (current_time - app_state.last_time);
            f64 frame_start_time = platform_get_absolute_time();

            if (!app_state.game_inst->update(app_state.game_inst, (f32)delta)) {
                P_FATAL("Game update failed. Shutting down");
                app_state.is_running = FALSE;
                break;
            }

            // Call the game's render routine
            if (!app_state.game_inst->render(app_state.game_inst, (f32)delta)) {
                P_FATAL("Game render failed. Shutting down");
                app_state.is_running = FALSE;
                break;
            }

            /// TODO: refactor packet creation
            render_packet packet;
            packet.delta_time = delta;
            renderer_draw_frame(&packet);

            f64 frame_end_time = platform_get_absolute_time();
            f64 frame_elapsed_time = frame_end_time - frame_start_time;
            running_time += frame_elapsed_time;
            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

            if (remaining_seconds > 0) {
                u64 remaining_ms = (remaining_seconds * 1000);

                // If there is time left
                // give it back to the OS
                b8 limit_frames = FALSE;
                if (remaining_ms > 0 && limit_frames) {
                    platform_sleep(remaining_ms - 1);
                }

                frame_count++;
            }

            /// NOTE: input update/state copying should always be handled
            ///       after any input should be recorded. IE before this line
            ///       As a safety, input is the last thing to be updated before the frame ends
            input_update(delta);

            // Update the last time
            app_state.last_time = current_time;
        }
    }
    app_state.is_running = FALSE; // ensure that we begin shutdown process with correct information

    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_unregister(EVENT_CODE_RESIZED, 0, application_on_resize);
    
    event_shutdown();
    input_shutdown();
    renderer_shutdown();
    platform_shutdown(&app_state.platform);
    P_INFO("APPLICATION SHUTTING DOWN");

    return TRUE;
}

// Get the dimensions (in pixels) of a framebuffer
void
application_get_framebuffer_size(u32* width, u32* height) {
    *width = app_state.width;
    *height = app_state.height;
}

// If we got the code to quit the application, handle it
// Otherwise return FALSE saying that we did not handle it
b8 
application_on_event(u16 code, void* sender, void* listener_inst, event_context context) {
    switch (code) {
        case EVENT_CODE_APPLICATION_QUIT: {
            P_INFO("EVENT_CODE_APPLICATION_QUIT received, shutting down\n");
            app_state.is_running = FALSE;
            return TRUE;
        }
    }

    return FALSE;
}

// Callback function for keyboard input
b8 
application_on_key(u16 code, void* sender, void* listener_inst, event_context context) {
    P_DEBUG("application_on_key fired");
    if (code == EVENT_CODE_KEY_PRESSED) {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_ESCAPE) {
            /// NOTE: technically firing an event to itself, but there may be other listeners
            event_context data = {};
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);

            // Block anything else from processing this
            return TRUE;
        } else if (key_code == KEY_A) {
            // Example for checking A key
            P_DEBUG("Explicit - A key pressed");
        } else {
            P_DEBUG("'%c' key pressed in window", key_code);
        }
    } else if (code == EVENT_CODE_KEY_RELEASED) {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_B) {
            P_DEBUG("Explicit - B key released");
        } else {
            P_DEBUG("'%c' key released in window", key_code);
        }
    }

    return FALSE;
}

b8 
application_on_resize(u16 code, void* sender, void* listener_inst, event_context context) {
    if (code == EVENT_CODE_RESIZED) {
        u16 width = context.data.u16[0];
        u16 height = context.data.u16[1];

        // Check if the width and height are different
        if (width != app_state.width || height != app_state.height) {
            app_state.width = width;
            app_state.height = height;

            P_DEBUG("Application resized [%i, %i]", width, height);
            
            // Handle minimization
            if (width == 0 || height == 0) {
                P_INFO("Window minimized. Suspending application");
                app_state.is_suspended = TRUE;
                return TRUE;
            } else {
                if (app_state.is_suspended) {
                    P_INFO("Window restored. Resuming application");
                    app_state.is_suspended = FALSE;
                }

                app_state.game_inst->on_resize(app_state.game_inst, width, height);
                renderer_on_resized(width, height);
            }
        }
   } 

   return FALSE;
}