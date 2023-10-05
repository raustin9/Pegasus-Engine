#include "game.h"
#include <entry.h>
// #include <platform/platform.h>
#include <core/pmemory.h>

// Definition of our function to create the game
b8 
create_game(game* out_game) {
    // Application config
    out_game->app_config.start_pos_x = 100;
    out_game->app_config.start_pos_y = 100;
    out_game->app_config.start_width = 1280;
    out_game->app_config.start_height = 720;
    out_game->app_config.name = "Pegasus Engine Testbed";

    out_game->initialize = game_initialize;
    out_game->update = game_update;
    out_game->render = game_render;
    out_game->on_resize = game_on_resize;

    out_game->state = pallocate(sizeof(game_state), MEMORY_TAG_GAME); 

    return TRUE;
}
