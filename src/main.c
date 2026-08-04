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
  WM_TAKE_FOCUS = get_atom("WM_TAKE_FOCUS");
  _NET_ACTIVE_WINDOW = get_atom("_NET_ACTIVE_WINDOW");
  _NET_WM_WINDOW_TYPE = get_atom("_NET_WM_WINDOW_TYPE");
  _NET_WM_WINDOW_TYPE_DIALOG = get_atom("_NET_WM_WINDOW_TYPE_DIALOG");
  _NET_WM_WINDOW_TYPE_UTILITY = get_atom("_NET_WM_WINDOW_TYPE_UTILITY");
  _NET_WM_WINDOW_TYPE_SPLASH = get_atom("_NET_WM_WINDOW_TYPE_SPLASH");
  /* Set root event mask */
  window_seteventmask(
      root,
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
      | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
      | XCB_EVENT_MASK_KEY_PRESS
      | XCB_EVENT_MASK_KEY_RELEASE
      | XCB_EVENT_MASK_FOCUS_CHANGE
      | XCB_EVENT_MASK_PROPERTY_CHANGE
  );
  /* Colormap stuff */
  colormap = create_colormap();
  inactive_pixel = get_pixel(INACTIVE_BORDER);
  active_pixel = get_pixel(ACTIVE_BORDER);
  /* Keyboard setup */
  xkb_context = create_xkb_context();
  xkb_keymap = create_xkb_keymap();
  xkb_state = create_xkb_state();
  for (uint32_t i = 0; i < NUM_KEYMAPS; i++)
    grab_keymap(_KEYMAPS[i].modifiers, _KEYMAPS[i].keysym);

  /* Clear clients */
  memset(clients, 0, MAX_CLIENTS*sizeof(client_t));

  /* Event loop */
  running = true;
  while (running) eventloop();

  /* Cleanup */
  free_colormap();
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
  screen_rect.width = _screen->width_in_pixels;
  screen_rect.height = _screen->height_in_pixels;
  return _screen;
}
static xcb_window_t get_root(void) { return screen->root; }
static xcb_colormap_t create_colormap(void) {
  xcb_colormap_t cmap = xcb_generate_id(connection);
  xcb_void_cookie_t cookie = xcb_create_colormap(
      connection, XCB_COLORMAP_ALLOC_NONE, cmap, root, screen->root_visual
  );
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(LOG_LEVEL_ERROR, "Failed to create colourmap (%d)", error_code);
  }
  return cmap;
}
static void free_colormap(void) {
  xcb_void_cookie_t cookie = xcb_free_colormap(connection, colormap);
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    int error_code = error->error_code;
    free(error);
    log_msg(LOG_LEVEL_ERROR, "Failed to free colourmap (%d)", error_code);
  }
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
    if (error) log_msg(
          LOG_LEVEL_ERROR,
          "Failed to get atom: %s (%d)", name, error->error_code
      );
    else log_msg(LOG_LEVEL_ERROR, "Failed to get atom: %s", name);
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
static uint32_t get_pixel(uint32_t color) {
  xcb_generic_error_t *error = NULL;
  xcb_alloc_color_cookie_t cookie = xcb_alloc_color(
      connection, colormap,
      ((float)((color >> 16) & 0xff)/(float)0xff)*0xffff,
      ((float)((color >> 8) & 0xff)/(float)0xff)*0xffff,
      ((float)(color & 0xff)/(float)0xff)*0xffff
  );
  xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(
      connection, cookie, &error
  );
  if (!reply) {
    if (error)
      log_msg(LOG_LEVEL_ERROR, "Failed to get pixel (%d)", error->error_code);
    else
      log_msg(LOG_LEVEL_ERROR, "Failed to get pixel");
  }
  uint32_t pixel = reply->pixel;
  free(reply);
  return pixel;
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
static void window_seteventmask(xcb_window_t window, uint32_t event_mask) {
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
static void window_setrect(xcb_window_t window, rect_t rect) {
  /* 
   * If this were in a packed struct, I could remove this value_list and just
   * reference it directly lol.
   */
  uint32_t value_list[4] = { rect.x, rect.y, rect.width, rect.height };
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
static void window_setborder(xcb_window_t window, uint32_t pixel) {
  uint32_t value_list[] = { BORDER_WIDTH };
  xcb_void_cookie_t cookie = xcb_configure_window(
      connection, window,
      XCB_CONFIG_WINDOW_BORDER_WIDTH, value_list
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
  cookie = xcb_change_window_attributes(
      connection, window,
      XCB_CW_BORDER_PIXEL, &pixel
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to change window attributes (%d)",
        error->error_code
    );
    free(error);
  }
  xcb_flush(connection);
}
static void window_focus(xcb_window_t window) {
  xcb_void_cookie_t cookie = xcb_set_input_focus(
      connection, XCB_INPUT_FOCUS_PARENT,
      window, XCB_CURRENT_TIME
  );
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to set input focus (%d)",
        error->error_code
    );
    free(error);
  }
  
  const xcb_client_message_event_t wm_event = {
    .response_type = XCB_CLIENT_MESSAGE,
    .format = 32,
    .window = window,
    .type = WM_PROTOCOLS,
    .data.data32 = { WM_TAKE_FOCUS, XCB_CURRENT_TIME, 0, 0, 0 }
  };
  cookie = xcb_send_event(
      connection,
      0, window,
      XCB_EVENT_MASK_NO_EVENT,
      (const char *)&wm_event
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to send WM_TAKE_FOCUS event (%d)",
        error->error_code
    );
    free(error);
  }

  const xcb_window_t w = window;
  cookie = xcb_change_property(
      connection,
      XCB_PROP_MODE_REPLACE, root,
      _NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW,
      32, 1, &w
  );
  error = xcb_request_check(connection, cookie);
  if (error) {
    log_msg(
        LOG_LEVEL_ERROR,
        "Failed to change _NET_ACTIVE_WINDOW property (%d)",
        error->error_code
    );
    free(error);
  }

  xcb_flush(connection);
}

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

/* Misc */
static void refresh_layout(void) {
  uint32_t normal_client_count = 0;
  int32_t main_client = -1;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type == CLIENT_NORMAL
      && clients[i].workspace == current_workspace
    ) {
      normal_client_count++;
      if (main_client < 0) main_client = i;
    }
  }
  if (!normal_client_count) return;
  if (focused_client >= 0) {
    if (
      clients[focused_client].type == CLIENT_NORMAL
      && clients[focused_client].workspace == current_workspace
    ) main_client = focused_client;
  }
  if (normal_client_count == 1) {
    window_setrect(
        clients[main_client].window,
        (rect_t){
          .x = 0, .y = 0,
          .width = screen_rect.width - BORDER_WIDTH*2,
          .height = screen_rect.height - BORDER_WIDTH*2
        }
    );
    return;
  }
  window_setrect(
      clients[main_client].window,
      (rect_t){
        .x = 0, .y = 0,
        .width = screen_rect.width/2 - BORDER_WIDTH,
        .height = screen_rect.height - BORDER_WIDTH*2
      }
  );
  size_t j = 0;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type == CLIENT_NORMAL
      && clients[i].workspace == current_workspace
      && i != (size_t)main_client
    ) {
      window_setrect(
          clients[i].window,
          (rect_t){
            .x = screen_rect.width/2 + BORDER_WIDTH,
            .y = j*(screen_rect.height/(normal_client_count-1)),
            .width = screen_rect.width/2 - BORDER_WIDTH,
            .height = screen_rect.height/(normal_client_count-1) - BORDER_WIDTH
          }
      );
      j++;
    }
  }
}
static void client_focus(int32_t client) {
  if (focused_client >= 0)
    window_setborder(clients[focused_client].window, inactive_pixel);
  focused_client = client;
  if (focused_client >= 0) {
    /*
     * It doesn't make much sense to focus clients that aren't in the current
     * workspace, but checking for it doesn't matter. If that changes, this
     * would be the place to add a check.
     */
    window_setborder(clients[focused_client].window, active_pixel);
    window_focus(clients[focused_client].window);
  }
  refresh_layout();
}

/* Keymap handlers */
static void handle_keymap_quit(
    xcb_key_press_event_t *event, keymap_data_t data
) { running = false; }
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
static void handle_keymap_cyclefocus(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  if (focused_client < 0) return;

  /* Prevent infinite loop if focus is hanging */
  bool found_one = false;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID
      && clients[i].workspace == current_workspace
    ) found_one = true;
  }
  if (!found_one) return;

  size_t cursor = (size_t)(focused_client+1);
  while (
    clients[cursor%MAX_CLIENTS].type == CLIENT_INVALID
    || clients[cursor%MAX_CLIENTS].workspace != current_workspace
  ) cursor++;
  client_focus(cursor%MAX_CLIENTS);
}
static void handle_keymap_setworkspace(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  xcb_void_cookie_t cookie;
  xcb_generic_error_t *error;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID
      && clients[i].workspace == current_workspace
    ) {
      cookie = xcb_unmap_window(connection, clients[i].window);
      error = xcb_request_check(connection, cookie);
      if (error) {
        log_msg(
            LOG_LEVEL_ERROR, "Failed to unmap windnow (%d)", error->error_code
        );
        free(error);
      }
    }
  }
  current_workspace = (uint32_t)(data.i32);
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID
      && clients[i].workspace == current_workspace
    ) {
      cookie = xcb_map_window(connection, clients[i].window);
      error = xcb_request_check(connection, cookie);
      if (error) {
        log_msg(
            LOG_LEVEL_ERROR, "Failed to map windnow (%d)", error->error_code
        );
        free(error);
      }
    }
  }
  xcb_flush(connection);
  client_focus(-1);
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
        clients[i].type != CLIENT_INVALID
        && clients[i].workspace == current_workspace
    ) {
      client_focus(i);
      break;
    }
  }
}
static void handle_keymap_movetoworkspace(
    xcb_key_press_event_t *event, keymap_data_t data
) {
  int32_t client = -1;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID
      && clients[i].window == event->child
    ) client = i;
  }
  if (client < 0) return;
  clients[client].workspace = data.i32;
  handle_keymap_setworkspace(NULL, data);
  client_focus(client);
}

/* XCB handlers */
static void handle_xcb_create_notify(xcb_create_notify_event_t *event) { }
static void handle_xcb_destroy_notify(xcb_destroy_notify_event_t *event) {
  log_msg(LOG_LEVEL_INFO, "Processing destroy notify...");
  if (focused_client >= 0) {
    if (clients[focused_client].window == event->window) {
      client_focus(-1);
      for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (
          clients[i].type != CLIENT_INVALID
          && clients[i].window != event->window
          && clients[i].workspace == current_workspace
        ) {
          client_focus(i);
          break;
        }
      }
    }
  }
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID
      && clients[i].window == event->window
    ) clients[i].type = CLIENT_INVALID;
  }
  refresh_layout();
}
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

  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (
      clients[i].type != CLIENT_INVALID && event->window == clients[i].window
    ) return;
  }
  client_t client = {
    .window = event->window,
    .rect = { .x = 0, .y = 0, .width = 0, .height = 0 },
    .type = CLIENT_NORMAL,
    .workspace = current_workspace
  };
  
  xcb_get_window_attributes_cookie_t attr_cookie = xcb_get_window_attributes(
      connection, event->window
  );
  xcb_get_window_attributes_reply_t *attr_reply =
      xcb_get_window_attributes_reply(
          connection, attr_cookie, &error
      );
  if (!attr_reply) {
    if (error) log_msg(
          LOG_LEVEL_ERROR,
          "Failed to get window attributes (%d)", error->error_code
      );
    else log_msg(LOG_LEVEL_ERROR, "Failed to get window attributes");
  }
  if (attr_reply->override_redirect) client.type = CLIENT_UNMANAGED;
  free(attr_reply);

  xcb_get_property_cookie_t prop_cookie = xcb_get_property(
      connection, 0, event->window,
      _NET_WM_WINDOW_TYPE, XCB_ATOM_ATOM,
      0, UINT32_MAX
  );
  xcb_get_property_reply_t *prop_reply = xcb_get_property_reply(
      connection, prop_cookie, &error
  );
  if (!prop_reply) {
    if (error) log_msg(
          LOG_LEVEL_ERROR,
          "Failed to get window _NET_WM_WINDOW_TYPE property (%d)",
          error->error_code
      );
    else log_msg(
          LOG_LEVEL_ERROR, "Failed to get window _NET_WM_WINDOW_TYPE property"
      );
  }
  xcb_atom_t *window_types = (xcb_atom_t *)xcb_get_property_value(prop_reply);
  size_t num_window_types =
      xcb_get_property_value_length(prop_reply) / sizeof(xcb_atom_t);
  for (size_t i = 0; i < num_window_types; i++) {
    if (
        window_types[i] == _NET_WM_WINDOW_TYPE_DIALOG
        || window_types[i] == _NET_WM_WINDOW_TYPE_UTILITY
        || window_types[i] == _NET_WM_WINDOW_TYPE_SPLASH
    ) client.type = CLIENT_FLOATING;
  }
  free(prop_reply);

  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].type == CLIENT_INVALID) {
      clients[i] = client;
      client_focus(i);
      break;
    }
  }
  refresh_layout();
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
static void handle_xcb_client_message(xcb_client_message_event_t *event) { }
static void handle_xcb_property_notify(xcb_property_notify_event_t *event) { }
