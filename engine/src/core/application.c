#include "application.h"
#include "game_types.h"

#include "platform/platform.h"
#include "core/pmemory.h"
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

b8
application_create(game* game_inst) {
    if (initialized) {
        P_ERROR("application_create called more than once");
        return FALSE;
    }

    app_state.game_inst = game_inst;

    // Initialize subsystems
    initialize_logging();

    // TODO: remove this
    P_FATAL("test message: %f", 3.14f);
    P_ERROR("test message: %f", 3.14f);
    P_WARN("test message: %f", 3.14f);
    P_INFO("test message: %f", 3.14f);
    P_DEBUG("test message: %f", 3.14f);
    P_TRACE("test message: %f", 3.14f);

    app_state.is_running = TRUE;
    app_state.is_suspended = FALSE;

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
        }
    }
    app_state.is_running = FALSE; // ensure that we begin shutdown process with correct information

    platform_shutdown(&app_state.platform);

    return TRUE;
}
