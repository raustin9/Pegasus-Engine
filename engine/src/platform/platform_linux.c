#include "platform.h"


#if P_PLATFORM_LINUX

#include "defines.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/event.h"
#include "containers/darray.h"
#include "renderer/vulkan/vulkan_platform.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/X.h>

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


keys translate_key(u32 x_keycode); // translate keycode from what X defines it as to what we want to use
                                   // we already use the Windows keycodes, so we are translating them to 
                                   // thos that we have already defined

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
                xcb_key_press_event_t *kb_event = (xcb_key_press_event_t *)event;
                b8 pressed = event->response_type == XCB_KEY_PRESS;

                xcb_keycode_t code = kb_event->detail;
                KeySym key_sym = XkbKeycodeToKeysym(state->display, (KeyCode)code, 0, code & ShiftMask ? 1 : 0);

                keys key = translate_key(key_sym);

                // Pass to input subsystem
                input_process_key(key, pressed);
            } break;
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                // mouse button presses and releases
                xcb_button_press_event_t *mouse_event = (xcb_button_press_event_t*)event;
                b8 pressed = event->response_type == XCB_BUTTON_PRESS;
                buttons mouse_button = BUTTON_MAX_BUTTONS;
                switch (mouse_event->detail) {
                    case XCB_BUTTON_INDEX_1:
                        mouse_button = BUTTON_LEFT;
                        break;
                    case XCB_BUTTON_INDEX_2:
                        mouse_button = BUTTON_MIDDLE;
                        break;
                    case XCB_BUTTON_INDEX_3:
                        mouse_button = BUTTON_RIGHT;
                        break;
                }

                // Pass to the input subsystem
                if (mouse_button != BUTTON_MAX_BUTTONS) {
                    input_process_button(mouse_button, pressed);
                }
            }
            case XCB_MOTION_NOTIFY: {
                // mouse movement
                xcb_motion_notify_event_t *mouse_event = (xcb_motion_notify_event_t *)event;

                // Pass to the input subsystem
                input_process_mouse_move(mouse_event->event_x, mouse_event->event_y);
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

// Get platform required extension names
void
platform_get_required_extension_names(const char*** ext_darray) {
    darray_push(*ext_darray, &"VK_KHR_xcb_surface"); // can use _xlib_ instead of _xcb_ if using xlib
}

// Long ass switch statement to
// translate X keycode definitions
// to the Windows ones
keys
translate_key(u32 x_keycode) {
    switch (x_keycode) {
        case XK_BackSpace:
            return KEY_BACKSPACE;
        case XK_Return:
            return KEY_ENTER;
        case XK_Tab:
            return KEY_TAB;
            //case XK_Shift: return KEY_SHIFT;
            //case XK_Control: return KEY_CONTROL;

        case XK_Pause:
            return KEY_PAUSE;
        case XK_Caps_Lock:
            return KEY_CAPITAL;

        case XK_Escape:
            return KEY_ESCAPE;

            // Not supported
            // case : return KEY_CONVERT;
            // case : return KEY_NONCONVERT;
            // case : return KEY_ACCEPT;

        case XK_Mode_switch:
            return KEY_MODECHANGE;

        case XK_space:
            return KEY_SPACE;
        case XK_Prior:
            return KEY_PRIOR;
        case XK_Next:
            return KEY_NEXT;
        case XK_End:
            return KEY_END;
        case XK_Home:
            return KEY_HOME;
        case XK_Left:
            return KEY_LEFT;
        case XK_Up:
            return KEY_UP;
        case XK_Right:
            return KEY_RIGHT;
        case XK_Down:
            return KEY_DOWN;
        case XK_Select:
            return KEY_SELECT;
        case XK_Print:
            return KEY_PRINT;
        case XK_Execute:
            return KEY_EXECUTE;
        // case XK_snapshot: return KEY_SNAPSHOT; // not supported
        case XK_Insert:
            return KEY_INSERT;
        case XK_Delete:
            return KEY_DELETE;
        case XK_Help:
            return KEY_HELP;

        case XK_Meta_L:
            return KEY_LWIN;  // TODO: not sure this is right
        case XK_Meta_R:
            return KEY_RWIN;
            // case XK_apps: return KEY_APPS; // not supported

            // case XK_sleep: return KEY_SLEEP; //not supported

        case XK_KP_0:
            return KEY_NUMPAD0;
        case XK_KP_1:
            return KEY_NUMPAD1;
        case XK_KP_2:
            return KEY_NUMPAD2;
        case XK_KP_3:
            return KEY_NUMPAD3;
        case XK_KP_4:
            return KEY_NUMPAD4;
        case XK_KP_5:
            return KEY_NUMPAD5;
        case XK_KP_6:
            return KEY_NUMPAD6;
        case XK_KP_7:
            return KEY_NUMPAD7;
        case XK_KP_8:
            return KEY_NUMPAD8;
        case XK_KP_9:
            return KEY_NUMPAD9;
        case XK_multiply:
            return KEY_MULTIPLY;
        case XK_KP_Add:
            return KEY_ADD;
        case XK_KP_Separator:
            return KEY_SEPARATOR;
        case XK_KP_Subtract:
            return KEY_SUBTRACT;
        case XK_KP_Decimal:
            return KEY_DECIMAL;
        case XK_KP_Divide:
            return KEY_DIVIDE;
        case XK_F1:
            return KEY_F1;
        case XK_F2:
            return KEY_F2;
        case XK_F3:
            return KEY_F3;
        case XK_F4:
            return KEY_F4;
        case XK_F5:
            return KEY_F5;
        case XK_F6:
            return KEY_F6;
        case XK_F7:
            return KEY_F7;
        case XK_F8:
            return KEY_F8;
        case XK_F9:
            return KEY_F9;
        case XK_F10:
            return KEY_F10;
        case XK_F11:
            return KEY_F11;
        case XK_F12:
            return KEY_F12;
        case XK_F13:
            return KEY_F13;
        case XK_F14:
            return KEY_F14;
        case XK_F15:
            return KEY_F15;
        case XK_F16:
            return KEY_F16;
        case XK_F17:
            return KEY_F17;
        case XK_F18:
            return KEY_F18;
        case XK_F19:
            return KEY_F19;
        case XK_F20:
            return KEY_F20;
        case XK_F21:
            return KEY_F21;
        case XK_F22:
            return KEY_F22;
        case XK_F23:
            return KEY_F23;
        case XK_F24:
            return KEY_F24;

        case XK_Num_Lock:
            return KEY_NUMLOCK;
        case XK_Scroll_Lock:
            return KEY_SCROLL;

        case XK_KP_Equal:
            return KEY_NUMPAD_EQUAL;

        case XK_Shift_L:
            return KEY_LSHIFT;
        case XK_Shift_R:
            return KEY_RSHIFT;
        case XK_Control_L:
            return KEY_LCONTROL;
        case XK_Control_R:
            return KEY_RCONTROL;
        // case XK_Menu: return KEY_LMENU;
        case XK_Menu:
            return KEY_RMENU;

        case XK_semicolon:
            return KEY_SEMICOLON;
        case XK_plus:
            return KEY_PLUS;
        case XK_comma:
            return KEY_COMMA;
        case XK_minus:
            return KEY_MINUS;
        case XK_period:
            return KEY_PERIOD;
        case XK_slash:
            return KEY_SLASH;
        case XK_grave:
            return KEY_GRAVE;

        case XK_a:
        case XK_A:
            return KEY_A;
        case XK_b:
        case XK_B:
            return KEY_B;
        case XK_c:
        case XK_C:
            return KEY_C;
        case XK_d:
        case XK_D:
            return KEY_D;
        case XK_e:
        case XK_E:
            return KEY_E;
        case XK_f:
        case XK_F:
            return KEY_F;
        case XK_g:
        case XK_G:
            return KEY_G;
        case XK_h:
        case XK_H:
            return KEY_H;
        case XK_i:
        case XK_I:
            return KEY_I;
        case XK_j:
        case XK_J:
            return KEY_J;
        case XK_k:
        case XK_K:
            return KEY_K;
        case XK_l:
        case XK_L:
            return KEY_L;
        case XK_m:
        case XK_M:
            return KEY_M;
        case XK_n:
        case XK_N:
            return KEY_N;
        case XK_o:
        case XK_O:
            return KEY_O;
        case XK_p:
        case XK_P:
            return KEY_P;
        case XK_q:
        case XK_Q:
            return KEY_Q;
        case XK_r:
        case XK_R:
            return KEY_R;
        case XK_s:
        case XK_S:
            return KEY_S;
        case XK_t:
        case XK_T:
            return KEY_T;
        case XK_u:
        case XK_U:
            return KEY_U;
        case XK_v:
        case XK_V:
            return KEY_V;
        case XK_w:
        case XK_W:
            return KEY_W;
        case XK_x:
        case XK_X:
            return KEY_X;
        case XK_y:
        case XK_Y:
            return KEY_Y;
        case XK_z:
        case XK_Z:
            return KEY_Z;

        default:
            return 0;        
    }
}

#endif /* P_PLATFORM_LINUX */
