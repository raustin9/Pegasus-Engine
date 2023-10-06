#include "application.h"
#include "game_types.h"

#include "platform/platform.h"
#include "core/pmemory.h"
#include "core/event.h"
#include "core/input.h"
#include "logger.h"

#include <string.h>

typedef struct application_state {
    game* game_inst;
    b8 is_running;
    b8 is_suspended;
    platform_state platform;
    i16 width;
    i16 height;
    f64 last_time;
} application_state;

static b8 initialized = FALSE;
static application_state app_state;

b8 application_on_event(u16 code, void* sender, void* listener_inst, event_context context);
b8 application_on_key(u16 code, void* sender, void* listener_inst, event_context context);

b8
application_create(game* game_inst) {
    if (initialized) {
        P_ERROR("application_create called more than once");
        return FALSE;
    }

    app_state.game_inst = game_inst;

    // Initialize subsystems
    initialize_logging();
    input_initialize();

    // TODO: remove this
    P_FATAL("test message: %f", 3.14f);
    P_ERROR("test message: %f", 3.14f);
    P_WARN("test message: %f", 3.14f);
    P_INFO("test message: %f", 3.14f);
    P_DEBUG("test message: %f", 3.14f);
    P_TRACE("test message: %f", 3.14f);

    app_state.is_running = TRUE;
    app_state.is_suspended = FALSE;

    // Initialize event system
    if (!event_initialize()) {
        P_ERROR("Event system could not initialize. Application cannot continue");
        return FALSE;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);

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
    char* mem_usage = get_memory_usage_str();
    P_INFO(mem_usage);
    pfree(mem_usage, strlen(mem_usage), MEMORY_TAG_STRING);

    while (app_state.is_running) {
        if (!platform_pump_messages(&app_state.platform)) {
            app_state.is_running = FALSE;
        }

        if (!app_state.is_suspended) {
            if (!app_state.game_inst->update(app_state.game_inst, (f32)0)) {
                P_FATAL("Game update failed. Shutting down");
                app_state.is_running = FALSE;
                break;
            }

            if (!app_state.game_inst->render(app_state.game_inst, (f32)0)) {
                P_FATAL("Game render failed. Shutting down");
                app_state.is_running = FALSE;
                break;
            }

            /// NOTE: input update/state copying should always be handled
            ///       after any input should be recorded. IE before this line
            ///       As a safety, input is the last thing to be updated before the frame ends
            input_update(0);
        }
    }
    app_state.is_running = FALSE; // ensure that we begin shutdown process with correct information

    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    
    event_shutdown();
    input_shutdown();
    platform_shutdown(&app_state.platform);
    P_INFO("APPLICATION SHUTTING DOWN");

    return TRUE;
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
            P_DEBUG("'%s' key pressed in window", key_code);
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