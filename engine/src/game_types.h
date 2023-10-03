#pragma once

#include "defines.h"
#include "core/application.h"

// Represents the basic game state for a game
// Called for creation by the application
typedef struct game {
    application_config app_config; // application conifiguration

    b8 (*initialize)(struct game* game_inst);                         // function pointer to game's initialize function
    b8 (*update)(struct game* game_inst, f32 delta_time);             // function pointer to game's update function
    b8 (*render)(struct game* game_inst, f32 delta_time);             // function pointer to game's render function
    void (*on_resize)(struct game* game_inst, u32 width, u32 height); // function pointer to handle window resizes

    void* state; // game-specific state. Created and managed by the game
} game;
