/* === Standards === */
/* ICCCM: https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html */
/* EWMH: https://specifications.freedesktop.org/wm/latest/index.html#id-1.2 */

/* === Includes === */
#include <stdbool.h>              /* For booleans */
#include <stdint.h>               /* For standard fixed-size integer types */
#include <stdlib.h>               /* For abort(), malloc(), free(), etc */
#include <string.h>               /* For strlen(), etc */
#include <errno.h>                /* For errno */
#include <stdio.h>                /* For console output */
#include <stdarg.h>               /* For variadic function arguments */
#include <unistd.h>               /* For execvp(), dup2(), close(), fork() */
#include <fcntl.h>                /* For open() */
#include <xcb/xcb.h>              /* X (windowing system) C Bindings */
#include <xcb/xcb_icccm.h>        /* XCB ICCCM compliance structures*/
#include <xkbcommon/xkbcommon.h>  /* X KeyBoard helper library */

/* NOTE: xcb_icccm.h calls wm_normal_hints struct xcb_size_hints_t */

/* === Structs/Enums === */
/* Log levels */
typedef enum {
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_ERROR
} log_level_t;
/* Keymap data */
typedef union {
  int i32;
  float f32;
  void *ptr;
} keymap_data_t;
/* Keymap */
typedef struct {
  uint16_t modifiers;
  xkb_keysym_t keysym;
  void (*handler)(xcb_key_press_event_t *event, keymap_data_t data);
  keymap_data_t data;
} keymap_t;
/* Window */
typedef struct {
  xcb_window_t window;
} window_t;
/* Workspace */
typedef struct {
  window_t main_window;
  window_t *side_windows;
  uint32_t num_side_windows;
} workspace_t;

/* === Event handler declarations === */
#define DECLARE_HANDLER(event, ident)\
static void handle_##ident (xcb_##ident##_event_t *event);\
static void event_handler_##event (xcb_generic_event_t *event) {\
  handle_##ident((xcb_##ident##_event_t *)event);\
}
DECLARE_HANDLER(CREATE_NOTIFY, create_notify)
DECLARE_HANDLER(DESTROY_NOTIFY, destroy_notify)
DECLARE_HANDLER(MAP_NOTIFY, map_notify)
DECLARE_HANDLER(UNMAP_NOTIFY, unmap_notify)
DECLARE_HANDLER(REPARENT_NOTIFY, reparent_notify)
DECLARE_HANDLER(CONFIGURE_NOTIFY, configure_notify)
DECLARE_HANDLER(GRAVITY_NOTIFY, gravity_notify)
DECLARE_HANDLER(MAP_REQUEST, map_request)
DECLARE_HANDLER(CONFIGURE_REQUEST, configure_request)
DECLARE_HANDLER(CIRCULATE_REQUEST, circulate_request)
DECLARE_HANDLER(KEY_PRESS, key_press)
DECLARE_HANDLER(KEY_RELEASE, key_release)
DECLARE_HANDLER(FOCUS_IN, focus_in)
DECLARE_HANDLER(FOCUS_OUT, focus_out)
#undef DECLARE_HANDLER
static void (*EVENT_HANDLERS[])(xcb_generic_event_t *) = {
#define ADD_HANDLER(event) [XCB_##event] = event_handler_##event,
  ADD_HANDLER(CREATE_NOTIFY)
  ADD_HANDLER(DESTROY_NOTIFY)
  ADD_HANDLER(MAP_NOTIFY)
  ADD_HANDLER(UNMAP_NOTIFY)
  ADD_HANDLER(REPARENT_NOTIFY)
  ADD_HANDLER(CONFIGURE_NOTIFY)
  ADD_HANDLER(GRAVITY_NOTIFY)
  ADD_HANDLER(MAP_REQUEST)
  ADD_HANDLER(CONFIGURE_REQUEST)
  ADD_HANDLER(CIRCULATE_REQUEST)
  ADD_HANDLER(KEY_PRESS)
  ADD_HANDLER(KEY_RELEASE)
  ADD_HANDLER(FOCUS_IN)
  ADD_HANDLER(FOCUS_OUT)
#undef ADD_HANDLER
};

/* == Keymap handler declarations */
static void handle_keymap_quit(
    xcb_key_press_event_t *event, keymap_data_t data
);
static void handle_keymap_spawnprocess(
    xcb_key_press_event_t *event, keymap_data_t data
);

/* === Function declarations === */
static void log_msg(log_level_t level, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
static void connect(void);
static void cleanup(void);
static void get_setup_info(void);
static xcb_atom_t get_atom(const char *name);
static void set_event_mask(xcb_window_t window, uint32_t event_mask);
static void init_xkb(void);
static void grab_keymap(uint16_t modifiers, xkb_keysym_t keysym);
static void change_window_rect(
    xcb_window_t window, uint16_t x, uint16_t y, uint16_t width, uint16_t height
);
static void reconfigure(void);
static void append_sidewindow(window_t window);
static void remove_sidewindow(uint32_t index);
static void set_border_colour(xcb_window_t window, uint32_t colour);

/* === Consts === */
#define LOGS 1 /* logging */
#define ANSI_LOGS 1 /* coloured logging with ANSI escape codes */
const char *LOG_LEVELS[] = {
#if ANSI_LOGS
  [LOG_LEVEL_INFO] = "\x1b[1;4;96mINFO\x1b[0m: ",
  [LOG_LEVEL_WARNING] = "\x1b[1;4;93mWARNING\x1b[0m: ",
  [LOG_LEVEL_ERROR] = "\x1b[1;4;91mERROR\x1b[0m: ",
#else
#define ADD_LEVEL(lvl) [LOG_LEVEL_##lvl] = #lvl ": ",
  ADD_LEVEL(INFO)
  ADD_LEVEL(WARNING)
  ADD_LEVEL(ERROR)
#undef ADD_LEVEL
#endif
};
#define MOD1 XCB_MOD_MASK_1
#define MOD4 XCB_MOD_MASK_4
#define SHIFT XCB_MOD_MASK_SHIFT
const char *termcmd[] = { "st", NULL };
const keymap_t KEYMAPS[] = {
  { MOD1|SHIFT, XKB_KEY_c, handle_keymap_quit, { .i32 = 0 } },
  { MOD1, XKB_KEY_Return, handle_keymap_spawnprocess, { .ptr = termcmd } },
};
#define NUM_KEYMAPS (sizeof(KEYMAPS)/sizeof(keymap_t))
#define BORDER_WIDTH 5

/* === Global state === */
static bool running = false;
static workspace_t workspace = { {0}, NULL, 0 };
static xcb_connection_t *connection = NULL;
static xcb_window_t root = 0;
static xcb_screen_t *screen = NULL;
static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;
static struct xkb_state *xkb_state = NULL;

/* === Entry point === */
int main(int argc, char *argv[]) {
  log_msg(LOG_LEVEL_INFO, "Connecting to X server...");
  connect();
  log_msg(LOG_LEVEL_INFO, "Collecting setup information...");
  get_setup_info();
  set_event_mask(
      root,
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
      | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
      | XCB_EVENT_MASK_KEY_PRESS
      | XCB_EVENT_MASK_KEY_RELEASE
      | XCB_EVENT_MASK_FOCUS_CHANGE
  );
  init_xkb();
  for (uint32_t i = 0; i < NUM_KEYMAPS; i++)
    grab_keymap(KEYMAPS[i].modifiers, KEYMAPS[i].keysym);

  /* Event loop */
  log_msg(LOG_LEVEL_INFO, "Starting event loop...");
  running = true;
  while (running) {
    xcb_generic_event_t *event = xcb_wait_for_event(connection);
    uint8_t type = event->response_type & ~0x80;
    if (type < (sizeof(EVENT_HANDLERS)/sizeof(EVENT_HANDLERS[0])))
      if (EVENT_HANDLERS[type])
        EVENT_HANDLERS[type](event);
    free(event);
  }

  log_msg(LOG_LEVEL_INFO, "Cleaning up...");
  cleanup();
  return 0;
}

/* == Keymap handler declarations */
static void handle_keymap_quit(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  running = false;
}
static void handle_keymap_spawnprocess(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  if (!fork()) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0)
      log_msg(
          LOG_LEVEL_ERROR,
          "Failed to open /dev/null (%s)", strerror(errno)
      );
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    execvp(((char **)data.ptr)[0], ((char **)data.ptr));
  }
}


/* === Function definitions === */
static void log_msg(log_level_t level, const char *format, ...) {
#if LOGS
  fprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, LOG_LEVELS[level]);
  va_list args;
  va_start(args, format);
  vfprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, format, args);
  va_end(args);
  fprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, "\n");
  if (level == LOG_LEVEL_ERROR) abort();
#endif
}
static void connect(void) {
  connection = xcb_connect(NULL, NULL);
  int connection_error = xcb_connection_has_error(connection);
  if (connection_error) {
    xcb_disconnect(connection);
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to connect to X server (%d)", connection_error
    );
  }
}
static void cleanup(void) {
  if (workspace.num_side_windows)
    free(workspace.side_windows);
  xkb_state_unref(xkb_state);
  xkb_keymap_unref(xkb_keymap);
  xkb_context_unref(xkb_context);
  xcb_disconnect(connection);
}
static void get_setup_info(void) {
  const xcb_setup_t *setup = xcb_get_setup(connection);
  if (!setup)
    log_msg(LOG_LEVEL_ERROR, "Failed to get setup information");
  log_msg(
      LOG_LEVEL_INFO, "setup.protocol_major_version = %d",
      setup->protocol_major_version
  );
  log_msg(
      LOG_LEVEL_INFO, "setup.protocol_minor_version = %d",
      setup->protocol_minor_version
  );
  xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup);
  screen = screen_iterator.data;
  log_msg(
      LOG_LEVEL_INFO, "screen.width_in_millimeters = %d",
      screen->width_in_millimeters
  );
  log_msg(
      LOG_LEVEL_INFO, "screen.height_in_millimeters = %d",
      screen->height_in_millimeters
  );
  log_msg(
      LOG_LEVEL_INFO, "screen.width_in_pixels = %d",
      screen->width_in_pixels
  );
  log_msg(
      LOG_LEVEL_INFO, "screen.height_in_pixels = %d",
      screen->height_in_pixels
  );
  root = screen->root;
}
static xcb_atom_t get_atom(const char *name) {
  xcb_generic_error_t *error = NULL;
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
      connection, 0, strlen(name), name
  );
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
      connection, cookie, &error
  );
  if (!reply) {
    if (error)
      log_msg(
          LOG_LEVEL_ERROR,
          "Failed to get atom: %s (%d)", name, error->error_code
      );
    else
      log_msg(LOG_LEVEL_ERROR, "Failed to get atom: %s", name);
  }
  xcb_atom_t atom = reply->atom;
  free(reply);
  if (!atom)
    log_msg(LOG_LEVEL_ERROR, "Failed to get atom: %s", name);
  else
    log_msg(LOG_LEVEL_INFO, "Got atom: %s", name);
  return atom;
}
static void set_event_mask(xcb_window_t window, uint32_t event_mask) {
  xcb_generic_error_t *error = NULL;
  xcb_void_cookie_t cookie = xcb_change_window_attributes(
      connection, window, XCB_CW_EVENT_MASK, &event_mask
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to change event mask of window %d (%d)",
        (int)window, error_code
    );
  }
}
static void init_xkb(void) {
  xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  xkb_keymap = xkb_keymap_new_from_names(
      xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS
  );
  xkb_state = xkb_state_new(xkb_keymap);
}
static void grab_keymap(uint16_t modifiers, xkb_keysym_t keysym) {
  char keyname[64];
  if (xkb_keysym_get_name(keysym, keyname, sizeof(keyname)) < 0)
    memcpy(keyname, "???\0", 4);
  log_msg(
      LOG_LEVEL_INFO,
      "Grabbing combination %s%s%s%s%s%s%s%s%s",
      modifiers & XCB_MOD_MASK_SHIFT ? "Shift+" : "",
      modifiers & XCB_MOD_MASK_LOCK ? "Capslock+" : "",
      modifiers & XCB_MOD_MASK_CONTROL ? "Ctrl+" : "",
      modifiers & XCB_MOD_MASK_1 ? "Alt+" : "",
      modifiers & XCB_MOD_MASK_2 ? "Numlock+" : "",
      modifiers & XCB_MOD_MASK_3 ? "Mod3+" : "",
      modifiers & XCB_MOD_MASK_4 ? "Super+" : "",
      modifiers & XCB_MOD_MASK_5 ? "AltGr+" : "",
      keyname
  );
    
  xkb_keycode_t xkb_keycode;
  xkb_keycode_t min = xkb_keymap_min_keycode(xkb_keymap);
  xkb_keycode_t max = xkb_keymap_max_keycode(xkb_keymap);
  bool found = false;
  for (xkb_keycode_t i = min; i <= max; i++) {
    int num_keysyms;
    const xkb_keysym_t *keysyms;
    num_keysyms =
      xkb_keymap_key_get_syms_by_level(xkb_keymap, i, 0, 0, &keysyms);
    for (int j = 0; j < num_keysyms; j++) {
      if (keysyms[j] == keysym) {
        found = true;
        xkb_keycode = i;
      }
    }
  }
  if (!found) log_msg(LOG_LEVEL_ERROR, "Couldn't find keysym %d", keysym);

  xcb_keycode_t keycode = (xcb_keycode_t)xkb_keycode;
  xcb_generic_error_t *error = NULL;
  xcb_void_cookie_t cookie = xcb_grab_key(
      connection,
      0,
      root,
      modifiers, keycode,
      XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(LOG_LEVEL_ERROR, "Failed to grab keys: (%d)", error_code
    );
  }
}
static void change_window_rect(
    xcb_window_t window, uint16_t x, uint16_t y, uint16_t width, uint16_t height
) {
  uint32_t value_list[4] = { x, y, width, height };
  xcb_void_cookie_t cookie = xcb_configure_window(
      connection, window,
      XCB_CONFIG_WINDOW_X
      | XCB_CONFIG_WINDOW_Y
      | XCB_CONFIG_WINDOW_WIDTH
      | XCB_CONFIG_WINDOW_HEIGHT,
      value_list
  );
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(LOG_LEVEL_ERROR, "Failed to configure window (%d)", error_code);
  }
}
static void reconfigure(void) {
  if (workspace.main_window.window != 0) {
    if (workspace.num_side_windows == 0)
      change_window_rect(
          workspace.main_window.window,
          0, 0,
          screen->width_in_pixels, screen->height_in_pixels
      );
    else {
      change_window_rect(
          workspace.main_window.window,
          0, 0,
          screen->width_in_pixels/2, screen->height_in_pixels
      );
      for (uint32_t i = 0; i < workspace.num_side_windows; i++) {
        change_window_rect(
            workspace.side_windows[i].window,
            screen->width_in_pixels/2,
            i*(screen->height_in_pixels/workspace.num_side_windows),
            screen->width_in_pixels/2,
            screen->height_in_pixels/workspace.num_side_windows
        );
      }
    }
  }
  xcb_flush(connection);
}
static void append_sidewindow(window_t window) {
  if (workspace.num_side_windows == 0)
    workspace.side_windows = malloc(sizeof(window_t));
  else
    workspace.side_windows = realloc(
        workspace.side_windows,
        sizeof(window_t) * (workspace.num_side_windows+1)
    );
  workspace.side_windows[workspace.num_side_windows] = window;
  workspace.num_side_windows++;
}
static void remove_sidewindow(uint32_t index) {
  for (uint32_t i = index+1; i < workspace.num_side_windows; i++)
    workspace.side_windows[i-1] = workspace.side_windows[i];
  workspace.num_side_windows--;
  if (workspace.num_side_windows)
    workspace.side_windows = realloc(
        workspace.side_windows,
        sizeof(window_t)*workspace.num_side_windows
    );
  else
    free(workspace.side_windows);
}
static void set_border_colour(xcb_window_t window, uint32_t colour) {
  xcb_generic_error_t *error = NULL;
  xcb_void_cookie_t cookie = xcb_change_window_attributes(
      connection, window, XCB_CW_EVENT_MASK, &colour
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to change event mask of window %d (%d)",
        (int)window, error_code
    );
  }
  uint32_t value_list[1] = { BORDER_WIDTH };
  cookie = xcb_configure_window(
      connection, window,
      XCB_CONFIG_WINDOW_BORDER_WIDTH,
      value_list
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(LOG_LEVEL_ERROR, "Failed to configure window (%d)", error_code);
  }
}

/* === Event handler definitions === */
static void handle_create_notify(xcb_create_notify_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing create notify...");
  if (workspace.main_window.window == 0)
    workspace.main_window.window = event->window;
  else append_sidewindow((window_t){event->window});
  reconfigure();
  set_border_colour(event->window, 0xffffff);
}
static void handle_destroy_notify(xcb_destroy_notify_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing destroy notify...");
  if (workspace.main_window.window == event->window) {
    workspace.main_window.window = 0;
    if (workspace.num_side_windows) {
      workspace.main_window = workspace.side_windows[0];
      remove_sidewindow(0);
    }
  } else {
    uint32_t index = 0;
    bool found = false;
    for (uint32_t i = 0; i < workspace.num_side_windows; i++) {
      if (workspace.side_windows[i].window == event->window) {
        found = true;
        index = i;
      }
    }
    if (found) remove_sidewindow(index);
  }
  reconfigure();
}
static void handle_map_notify(xcb_map_notify_event_t *event) { }
static void handle_unmap_notify(xcb_unmap_notify_event_t *event) { }
static void handle_reparent_notify(xcb_reparent_notify_event_t *event) { }
static void handle_configure_notify(xcb_configure_notify_event_t *event) { }
static void handle_gravity_notify(xcb_gravity_notify_event_t *event) { }
static void handle_map_request(xcb_map_request_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing map request...");
  xcb_void_cookie_t cookie = xcb_map_window(connection, event->window);
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(LOG_LEVEL_ERROR, "Failed to map window (%d)", error->error_code);
    free(error);
  }
  xcb_flush(connection);
}
static void handle_configure_request(xcb_configure_request_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing configure request...");

  uint32_t value_list[7];
  uint8_t num_values = 0;
  if (event->value_mask & XCB_CONFIG_WINDOW_X)
    value_list[num_values++] = event->x;
  if (event->value_mask & XCB_CONFIG_WINDOW_Y)
    value_list[num_values++] = event->y;
  if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
    value_list[num_values++] = event->width;
  if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
    value_list[num_values++] = event->height;
  if (event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
    value_list[num_values++] = event->border_width;
  if (event->value_mask & XCB_CONFIG_WINDOW_SIBLING)
    value_list[num_values++] = event->sibling;
  if (event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
    value_list[num_values++] = event->stack_mode;

  xcb_void_cookie_t cookie = xcb_configure_window(
      connection, event->window,
      event->value_mask, value_list
  );
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to configure window (%d)",
        error->error_code
    );
    free(error);
  }
  xcb_flush(connection);
}
static void handle_circulate_request(xcb_circulate_request_event_t *event) { }
static void handle_key_press(xcb_key_press_event_t *event) {
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb_state, event->detail);
  for (uint32_t i = 0; i < NUM_KEYMAPS; i++)
    if ((event->state == KEYMAPS[i].modifiers) && keysym == KEYMAPS[i].keysym)
      KEYMAPS[i].handler(event, KEYMAPS[i].data);
}
static void handle_key_release(xcb_key_release_event_t *event) { }
static void handle_focus_in(xcb_focus_in_event_t *event) { }
static void handle_focus_out(xcb_focus_out_event_t *event) { }
