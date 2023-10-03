#pragma once

#include "defines.h"

struct game;

typedef struct application_config {
    i16 start_pos_x;  // Window starting position x axis
    i16 start_pos_y;  // Window starting position y axis
    i16 start_width;  // Window starting width
    i16 start_height; // Window starting height
    char *name;       // Application name, if applicable
} application_config;

P_API b8 application_create(struct game* game_inst);

P_API b8 application_run();
