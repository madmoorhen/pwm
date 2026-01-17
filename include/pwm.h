/* Include guard */
#ifndef PWM_H
#define PWM_H

/* Includes */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xkbcommon/xkbcommon.h>
#include <config.h>
#include <logging.h>

/* Global state */
static bool running = false;
static xcb_connection_t *connection = NULL;
static const xcb_setup_t *setup = NULL;
static xcb_screen_t *screen = NULL;
static xcb_window_t root = 0;
static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;
static struct xkb_state *xkb_state = NULL;

/* Setup */
static xcb_connection_t *get_connection(void);
static void disconnect(void);
static const xcb_setup_t *get_setup(void);
static xcb_screen_t *get_screen(void);
static xcb_window_t get_root(void);
static xcb_atom_t get_atom(const char *name);
static void log_setup_info(void);
static void eventloop(void);

/* Manipulating windows */
static void set_event_mask(xcb_window_t window, uint32_t event_mask);
static void set_window_rect(
    xcb_window_t window, uint16_t x, uint16_t y, uint16_t width, uint16_t height
);

/* Keyboard */
static struct xkb_context *create_xkb_context(void);
static struct xkb_keymap *create_xkb_keymap(void);
static struct xkb_state *create_xkb_state(void);
static void unref_xkb_context(void);
static void unref_xkb_keymap(void);
static void unref_xkb_state(void);
static void grab_keymap(uint16_t modifiers, xkb_keysym_t keysym);

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
/* Keymap handlers */
static void handle_keymap_quit(
    xcb_key_press_event_t *event, keymap_data_t data
);
static void handle_keymap_spawnprocess(
    xcb_key_press_event_t *event, keymap_data_t data
);
/* Keymaps */
const keymap_t _KEYMAPS[] = { KEYMAPS };
#define NUM_KEYMAPS (sizeof(_KEYMAPS)/sizeof(keymap_t))

/* XCB handlers */
#define DECLARE_HANDLER(event, ident)\
static void handle_xcb_##ident (xcb_##ident##_event_t *event);\
static void event_handler_##event (xcb_generic_event_t *event) {\
  handle_xcb_##ident((xcb_##ident##_event_t *)event);\
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

#endif /* PWM_H */
