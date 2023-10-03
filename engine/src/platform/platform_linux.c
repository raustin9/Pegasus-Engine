#include "platform.h"

#if P_PLATFORM_LINUX

#include "defines.h"
#include "core/logger.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Handle time depending on distribution difference
#if _POSIX_C_SOURCE >= 199309L
#include <sys/time.h> // nanosleep
#else
#include <unistd.h> // usleep
#endif /* _POSIX_C_SOURCE */

// Linux internal state
typedef struct internal_state {
    Display *display;
    xcb_connection_t *connection;
    xcb_window_t window;
    xcb_screen_t *screen;
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_win;
} internal_state;

// Application startup behavior
b8 
platform_startup(
    platform_state* pstate,
    const char* application_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height) { 
 
    // Create internal state
    pstate->internal_state = malloc(sizeof(internal_state));
    internal_state *state = (internal_state *)pstate->internal_state;

    // Connect to X
    state->display = XOpenDisplay(NULL);

    // Turn off key repeats
    XAutoRepeatOff(state->display);

    // Retrieve the connection from the display
    state->connection = XGetXCBConnection(state->display);

    if (xcb_connection_has_error(state->connection)) {
        P_FATAL("Failed to connect to X server via XCB");
        return FALSE;
    }

    // Get data from the X server
    const struct xcb_setup_t *setup = xcb_get_setup(state->connection);

    // Loop through screens using iterator
    int screen_p = 0;
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (i32 s = screen_p; s > 0; s--) {
        xcb_screen_next(&it);
    }

    // After screens ahve been looped through, assign it
    state->screen = it.data;

    // Generate XID for the window to be created
    state->window = xcb_generate_id(state->connection);

    // Register event types
    // XCB_CW_BACK_PIXEL = filling then window bg with a single color
    // XCB_CW_EVENT_MASK is required
    u32 event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // Listen for keyboard and mouse buttons
    u32 event_values = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | 
                       XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | 
                       XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION | 
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // Values to be sent to XCB
    u32 value_list[] = {state->screen->black_pixel, event_values};

    // Create the window
    xcb_void_cookie_t cookie = xcb_create_window(
            state->connection,
            XCB_COPY_FROM_PARENT,
            state->window,
            state->screen->root,
            x,
            y,
            width,
            height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            state->screen->root_visual,
            event_mask,
            value_list);

    // Change the title
    xcb_change_property(
            state->connection,
            XCB_PROP_MODE_REPLACE,
            state->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8, // data should be 8 bits at a time (not bytes)
            strlen(application_name),
            application_name);

    // When the server attempts to destroy the window, tell
    // the server that it needs to notify our client
    xcb_intern_atom_cookie_t wm_delete_cookie = xcb_intern_atom(
            state->connection,
            0,
            strlen("WM_DELETE_WINDOW"),
            "WM_DELETE_WINDOW");
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
            state->connection,
            0,
            strlen("WM_PROTOCOLS"),
            "WM_PROTOCOLS");

    // Procedures that occur when the evens above occur
    xcb_intern_atom_reply_t *wm_delete_reply = xcb_intern_atom_reply(
            state->connection,
            wm_delete_cookie,
            NULL);
    xcb_intern_atom_reply_t *wm_protocols_reply = xcb_intern_atom_reply(
            state->connection,
            wm_protocols_cookie,
            NULL);

    state->wm_delete_win = wm_delete_reply->atom;
    state->wm_protocols = wm_protocols_reply->atom;

    xcb_change_property(
            state->connection,
            XCB_PROP_MODE_REPLACE,
            state->window,
            wm_protocols_reply->atom,
            4,
            8, // data should be 8 bits at a time (not bytes)
            32,
            &wm_delete_reply->atom);

    // map window to the screen
    xcb_map_window(state->connection, state->window);

    // Flush the stream
    i32 stream_result = xcb_flush(state->connection);
    if (stream_result <= 0) {
        P_FATAL("An error occurered when flushing the stream: %d", stream_result);
        return FALSE;
    }

    return TRUE;
}

// Shutdown behavior when the application closes
void 
platform_shutdown(platform_state *pstate) {
    // Simply cold cast to the known type
    internal_state *state = (internal_state *)pstate->internal_state;

    // Turn key repeats back on since turning them off is global
    XAutoRepeatOn(state->display);

    xcb_destroy_window(state->connection, state->window);
}


b8 
platform_pump_messages(platform_state* pstate) {
    internal_state *state = (internal_state*)pstate->internal_state;

    xcb_generic_event_t *event;
    xcb_client_message_event_t *cm;

    b8 quit_flagged = FALSE;

    // Poll the events
    // event = xcb_poll_for_event(state->connection);
    while (event != 0) {
        event = xcb_poll_for_event(state->connection);
        if (event == 0) {
            break;
        } 

        switch (event->response_type & ~0x80) {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: {
                // handle key presses and releases
            } break;
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                // mouse button presses and releases
            }
            case XCB_MOTION_NOTIFY: {
                // mouse movement
            }
            case XCB_CONFIGURE_NOTIFY: {
                //  Resizing
            } break;
            case XCB_CLIENT_MESSAGE: {
                cm = (xcb_client_message_event_t*)event;
                
                P_INFO("Should be destroying window");
                // Window close
                if (cm->data.data32[0] == state->wm_delete_win) {
                    quit_flagged = TRUE;
                }
            } break;
            default:
                // someting else
                break;
        }

        free(event);
    }

    return !quit_flagged;
}

// Deal with platform memory
void* platform_allocate(u64 size, b8 aligned) { 
    return malloc(size);
}
void platform_free(void* block, b8 aligned) { 
    free(block);
}
void* platform_zero_memory(void* block, u64 size) { 
    return memset(block, 0, size);
}
void* platform_copy_memory(void* dest, const void* source, u64 size) {
    return memcpy(dest, source, size);
}
void* platform_set_memory(void* dest, i32 value, u64 size) {  
    return memset(dest, value, size);
}

// Write colored text to the console
void platform_console_write(const char* message, u8 color) { 
    // FATAL, ERROR, WARN, INFO, DEBUG, TRACE
    const char* color_strings[] = {
        "0;41",
        "1;31",
        "1;33",
        "1;32",
        "1;34",
        "1;37"
    };

    printf("\033[%sm%s\033[0m", color_strings[color], message);
}
void 
platform_console_write_error(const char* message, u8 color) {
    // FATAL, ERROR, WARN, INFO, DEBUG, TRACE
    const char* color_strings[] = {
        "0;41",
        "1;31",
        "1;33",
        "1;32",
        "1;34",
        "1;37"
    };

    // Potentially: use stderr
    printf("\033[%sm%s\033[0m", color_strings[color], message);
}

// Get the time
f64 
platform_get_absolute_time() {  
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return now.tv_sec + now.tv_nsec * 0.000000001;
}

// Sleep on the thread for the provided ms. This blocks the main thread.
// This should only be used for giving time back to the OS for unused update power
// Therefore it is not exported
void 
platform_sleep(u64 ms) {  
    #if _POSIX_C_SOURCE >= 199309L
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000 * 1000;
        nanosleep(&ts, 0);
    #else
        if (ms >= 1000) {
            sleep(ms / 1000);
        }
        usleep((ms % 1000) * 1000);
    #endif
}

#endif /* P_PLATFORM_LINUX */
