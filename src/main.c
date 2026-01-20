/* Includes */
#include <pwm.h> /* To implement */

/* Entry point */
int main(int argc, char *argv[]) {
  /* Setup */
  connection = get_connection();
  setup = get_setup();
  screen = get_screen();
  root = get_root();
  log_setup_info();
  /* Get atoms */
  WM_PROTOCOLS = get_atom("WM_PROTOCOLS");
  WM_DELETE_WINDOW = get_atom("WM_DELETE_WINDOW");
  /* Set root event mask */
  set_event_mask(
      root,
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
      | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
      | XCB_EVENT_MASK_KEY_PRESS
      | XCB_EVENT_MASK_KEY_RELEASE
      | XCB_EVENT_MASK_FOCUS_CHANGE
  );
  /* Keyboard setup */
  xkb_context = create_xkb_context();
  xkb_keymap = create_xkb_keymap();
  xkb_state = create_xkb_state();
  for (uint32_t i = 0; i < NUM_KEYMAPS; i++)
    grab_keymap(_KEYMAPS[i].modifiers, _KEYMAPS[i].keysym);

  /* Event loop */
  running = true;
  while (running) eventloop();

  /* Cleanup */
  unref_xkb_state();
  unref_xkb_keymap();
  unref_xkb_context();
  disconnect();
  return 0;
}

/* Setup */
static xcb_connection_t *get_connection(void) {
  xcb_connection_t *_connection = xcb_connect(NULL, NULL);
  int error = xcb_connection_has_error(_connection);
  if (error) {
    xcb_disconnect(_connection);
    log_msg(LOG_LEVEL_ERROR, "Failed to connect to X server (%d)", error);
  }
  return _connection;
}

static void disconnect(void) {
  xcb_disconnect(connection);
}
static const xcb_setup_t *get_setup(void) {
  const xcb_setup_t *_setup =  xcb_get_setup(connection);
  if (!_setup)
    log_msg(LOG_LEVEL_ERROR, "Failed to get setup information");
  return _setup;
}
static xcb_screen_t *get_screen(void) {
  /* This gets the first screen blindly (not good for several monitors) */
  xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup);
  xcb_screen_t *_screen = screen_iterator.data;
  if (!_screen)
    log_msg(LOG_LEVEL_ERROR, "Failed to get first screen");
  return _screen;
}
static xcb_window_t get_root(void) {
  return screen->root;
}
static xcb_atom_t get_atom(const char *name) {
  /*
   * It would be better to query for all atoms before reading replies, making
   * the most of XCB's asynchronous API. It would also be more difficult, and it
   * only has to be done once, so performance isn't that much of an issue
   */
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
static void log_setup_info(void) {
  log_msg(
      LOG_LEVEL_INFO, "setup.protocol_major_version = %d",
      setup->protocol_major_version
  );
  log_msg(
      LOG_LEVEL_INFO, "setup.protocol_minor_version = %d",
      setup->protocol_minor_version
  );
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
}
static void eventloop(void) {
  xcb_generic_event_t *event = xcb_wait_for_event(connection);
  uint8_t type = event->response_type & ~0x80;
  if (type < (sizeof(EVENT_HANDLERS)/sizeof(EVENT_HANDLERS[0])))
    if (EVENT_HANDLERS[type])
      EVENT_HANDLERS[type](event);
  free(event);
}

/* Manipulating windows */
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
static void set_window_rect(
    xcb_window_t window, uint16_t x, uint16_t y, uint16_t width, uint16_t height
);

/* Keyboard */
static struct xkb_context *create_xkb_context(void) {
  return xkb_context_new(XKB_CONTEXT_NO_FLAGS);
}
static struct xkb_keymap *create_xkb_keymap(void) {
  return xkb_keymap_new_from_names(
      xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS
  );
}
static struct xkb_state *create_xkb_state(void) {
  return xkb_state_new(xkb_keymap);
}
static void unref_xkb_context(void) {
  xkb_context_unref(xkb_context);
}
static void unref_xkb_keymap(void) {
  xkb_keymap_unref(xkb_keymap);
}
static void unref_xkb_state(void) {
  xkb_state_unref(xkb_state);
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

/* Keymap handlers */
static void handle_keymap_quit(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  running = false;
}
static void handle_keymap_destroy(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  xcb_generic_error_t *error = NULL;
  const xcb_client_message_event_t wm_event = {
    .response_type = XCB_CLIENT_MESSAGE,
    .format = 32,
    .window = event->child,
    .type = WM_PROTOCOLS,
    .data.data32 = { WM_DELETE_WINDOW, XCB_CURRENT_TIME, 0, 0, 0 }
  };
  xcb_void_cookie_t cookie = xcb_send_event(
      connection,
      0, event->child,
      XCB_EVENT_MASK_NO_EVENT,
      (const char *)&wm_event
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to send WM_DELETE_WINDOW event (%d)", error_code
    );
  }
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

/* XCB handlers */
static void handle_xcb_create_notify(xcb_create_notify_event_t *event) { }
static void handle_xcb_destroy_notify(xcb_destroy_notify_event_t *event) { }
static void handle_xcb_map_notify(xcb_map_notify_event_t *event) { }
static void handle_xcb_unmap_notify(xcb_unmap_notify_event_t *event) { }
static void handle_xcb_reparent_notify(xcb_reparent_notify_event_t *event) { }
static void handle_xcb_configure_notify(xcb_configure_notify_event_t *event) { }
static void handle_xcb_gravity_notify(xcb_gravity_notify_event_t *event) { }
static void handle_xcb_map_request(xcb_map_request_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing map request...");
  xcb_void_cookie_t cookie = xcb_map_window(connection, event->window);
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(LOG_LEVEL_ERROR, "Failed to map window (%d)", error->error_code);
    free(error);
  }
  xcb_flush(connection);
}
static void handle_xcb_configure_request(xcb_configure_request_event_t *event) {
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
static void handle_xcb_circulate_request(xcb_circulate_request_event_t *event) { }
static void handle_xcb_key_press(xcb_key_press_event_t *event) {
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb_state, event->detail);
  for (uint32_t i = 0; i < NUM_KEYMAPS; i++)
    if ((event->state == _KEYMAPS[i].modifiers) && keysym == _KEYMAPS[i].keysym)
      _KEYMAPS[i].handler(event, _KEYMAPS[i].data);
}
static void handle_xcb_key_release(xcb_key_release_event_t *event) { }
static void handle_xcb_focus_in(xcb_focus_in_event_t *event) { }
static void handle_xcb_focus_out(xcb_focus_out_event_t *event) { }
