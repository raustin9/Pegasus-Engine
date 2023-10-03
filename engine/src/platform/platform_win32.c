#include "platform/platform.h"


// Windows platform layer
// Only compile if building on windows
#if P_PLATFORM_WINDOWS
#include "core/logger.h"


#include <windows.h>
#include <windowsx.h> // param input extraction
#include <stdlib.h>

// Define the structure for the internal state
typedef struct internal_state {
  HINSTANCE h_instance;
  HWND hwnd;
} internal_state;

// Clock
static f64 clock_frequency;
static LARGE_INTEGER start_time;

LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

// Startup behavior for Windows application startup
b8 platform_startup(
  platform_state* pstate,
  const char* application_name,
  i32 x,
  i32 y,
  i32 width,
  i32 height
) {
  pstate->internal_state = malloc(sizeof(internal_state));
  internal_state *state = (internal_state*)pstate->internal_state;

  state->h_instance = GetModuleHandle(0);

  // Setup and register window class
  HICON icon = LoadIcon(state->h_instance, IDI_APPLICATION);
  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_DBLCLKS; // get double clicks
  wc.lpfnWndProc = win32_process_message; // pointer to window procedure
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = state->h_instance;
  wc.hIcon = icon;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;                    // transparent
  wc.lpszClassName = "pegasus_window_class";  // need to know this string when we create our window

  if (!RegisterClassA(&wc)) {
    MessageBoxA(0, "Window registration failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
    return FALSE;
  }

  // Create the window
  u32 client_x = x;
  u32 client_y = y;
  u32 client_width = width;
  u32 client_height = height;

  u32 window_x = client_x;
  u32 window_y = client_y;
  u32 window_width = client_width;
  u32 window_height = client_height;

  u32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
  u32 window_ex_style = WS_EX_APPWINDOW;

  window_style |= WS_MAXIMIZEBOX;
  window_style |= WS_MINIMIZEBOX;
  window_style |= WS_THICKFRAME;

  // Get size of the border
  RECT border_rect = {0,0,0,0};
  AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

  // In this case the border rectanglei s negative
  window_x += border_rect.left;
  window_y += border_rect.top;

  // Grow by the size of the OS border
  window_width += border_rect.right - border_rect.left;
  window_height += border_rect.bottom - border_rect.top;

  // actually create the window
  HWND handle = CreateWindowExA(
    window_ex_style, "pegasus_window_class", application_name,
    window_style, window_x, window_y, window_width, window_height,
    0, 0, state->h_instance, 0
  );

  if (handle == 0) {
    MessageBoxA(NULL, "Window creation failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
    P_FATAL("Window creation failed");
    return FALSE;
  } else {
    state->hwnd = handle;
  }

  // Show the window
  b32 should_activate = 1; // if the window should not take input, this should be false
  i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
  // If initially minimized, use SW_MINIMIZE : SW_SHOWMINNOACTIVE
  // If initially maximized, use SW_SHOWMAXIMIZED : SW_MAXIMIZE
  ShowWindow(state->hwnd, show_window_command_flags);

  // Set the start time of application and setup the clock
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  clock_frequency = 1.0 / (f64)frequency.QuadPart;
  QueryPerformanceCounter(&start_time);

  return TRUE;
}

// Shutdown behavior for the windows platform
void
platform_shutdown(platform_state *pstate) {
  // Simply cold-cast to the known type
  internal_state *state = (internal_state*)pstate->internal_state;

  if (state->hwnd) {
    DestroyWindow(state->hwnd);
    state->hwnd = 0;
  }
}

// Pump messages to the application
// Will be called once every game loop
b8
platform_pump_messages(platform_state *pstate) {
  MSG message;

  // Takes messages from the queue and pump it to the application
  while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  return TRUE;
}

// MEMORY FUNCTIONS //
// TODO: change to custom allocator
void*
platform_allocate(u64 size, b8 aligned) {
  return malloc(size);
}
void
platform_free(void* block, b8 aligned) {
  free(block);
}
void*
platform_zero_memory(void *block, u64 size) {
  return memset(block, 0, size);
}
void*
platform_copy_memory(void* dest, const void* source, u64 size) {
  return memcpy(dest, source, size);
}
void*
platform_set_memory(void* dest, i32 value, u64 size) {
  return memset(dest, value, size);
}

// WRITE TO THE CONSOLE //
void
platform_console_write(const char* message, u8 color) {
  HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  // FATAL, ERROR, WARN, INFO, DEBUG, TRACE
  static u8 levels[6] = {64, 4, 6, 2, 1, 8}; // colors corresponding to severity level
  SetConsoleTextAttribute(console_handle, levels[color]);

  OutputDebugStringA(message);
  u64 length = strlen(message);
  LPDWORD number_written = 0; // pointer to number of chars written
  WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, (DWORD)length, number_written, 0);
}

// Write errors to the console
void
platform_console_write_error(const char* message, u8 color) {
  HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
  // FATAL, ERROR, WARN, INFO, DEBUG, TRACE
  static u8 levels[6] = {64, 4, 6, 2, 1, 8}; // colors corresponding to severity level
  SetConsoleTextAttribute(console_handle, levels[color]);

  OutputDebugStringA(message);
  u64 length = strlen(message);
  LPDWORD number_written = 0; // pointer to number of chars written
  WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)length, number_written, 0);
}

// TIME //
f64
platform_get_absolute_time() {
  LARGE_INTEGER now_time;
  QueryPerformanceCounter(&now_time);
  return (f64)now_time.QuadPart * clock_frequency;
}

// Sleep
void
platform_sleep(u64 ms) {
  Sleep(ms);
}


LRESULT CALLBACK win32_process_message(
  HWND hwnd, 
  u32 msg, 
  WPARAM w_param, 
  LPARAM l_param
) {
  switch(msg) {
    case WM_ERASEBKGND:
      // Notify the OS that erasing the screen will be handling the application
      return 1;
    case WM_CLOSE:
      // TODO: fire an event for the application to quit
      DestroyWindow(hwnd);
      // return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_SIZE: {
      // get updated size of the window
      // RECT r;
      // GetClientRect(hwnd, &r);
      // u32 width = r.right - r.left;
      // u32 height = r.bottom - r.top;


    } break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      // Key pressed/released
      // b8 pressd = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
      // TODO: input processing


    } break;
    case WM_MOUSEMOVE: {
      // i32 x_position = GET_X_LPARAM(l_param);
      // i32 y_position = GET_Y_LPARAM(l_param);
      // // TODO: input processing
    } break;
    case WM_MOUSEWHEEL: {
      // i32 z_delta = GET_WHEEL_DELTA_WPARAM(w_param);
      // if (z_delta != 0) {
      //   // flatten input to -1 or 1
      //   z_delta = (z_delta < 0) ? -1 : 1;
      //   // TODO: input processing
      // }
    } break;

    // Mouse buttons
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
      // b8 pressed = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN);
      // // TODO: input processing
    } break;
  }

  return DefWindowProcA(hwnd, msg, w_param, l_param);
}

#endif // P_PLATFORM_WINDOWS
