/* Include guard */
#ifndef CONFIG_H
#define CONFIG_H

/* Logging */
#define LOGS 1      /* Enable logging */
#define ANSI_LOGS 1 /* Enable formatted logs with ANSI escape codes */

/* Colors */
#define INACTIVE_BORDER 0x111111
#define ACTIVE_BORDER 0xb3ebf2

/* Keymaps - keys*/
#define SHIFT XCB_MOD_MASK_SHIFT
#define LOCK XCB_MOD_MASK_LOCK
#define CONTROL XCB_MOD_MASK_CONTROL
#define MOD1 XCB_MOD_MASK_1
#define MOD2 XCB_MOD_MASK_2
#define MOD3 XCB_MOD_MASK_3
#define MOD4 XCB_MOD_MASK_4
#define MOD5 XCB_MOD_MASK_5
/* Keymaps - commands */
static const char *termcmd[] = { "alacritty", (void *)(0) };
static const char *dmenucmd[] = { "dmenu_run", (void *)(0) };
static const char *browsercmd[] = { "brave", (void *)(0) };
/* Keymaps */
#define KEYMAPS \
    { MOD4|SHIFT, XKB_KEY_c, handle_keymap_quit, { .i32 = 0 } },\
    { MOD4|SHIFT, XKB_KEY_q, handle_keymap_destroy, { .i32 = 0 } },\
    { MOD4, XKB_KEY_Return, handle_keymap_spawnprocess, { .ptr = termcmd } },\
    { MOD4, XKB_KEY_d, handle_keymap_spawnprocess, { .ptr = dmenucmd } },\
    { MOD4, XKB_KEY_b, handle_keymap_spawnprocess, { .ptr = browsercmd } },\
    { MOD4, XKB_KEY_space, handle_keymap_cyclefocus, { .i32 = 0 } },\
    { MOD4, XKB_KEY_0, handle_keymap_setworkspace, { .i32 = 0 } },\
    { MOD4, XKB_KEY_1, handle_keymap_setworkspace, { .i32 = 1 } },\
    { MOD4, XKB_KEY_2, handle_keymap_setworkspace, { .i32 = 2 } },\
    { MOD4, XKB_KEY_3, handle_keymap_setworkspace, { .i32 = 3 } },\
    { MOD4, XKB_KEY_4, handle_keymap_setworkspace, { .i32 = 4 } },\
    { MOD4, XKB_KEY_5, handle_keymap_setworkspace, { .i32 = 5 } },\
    { MOD4, XKB_KEY_6, handle_keymap_setworkspace, { .i32 = 6 } },\
    { MOD4, XKB_KEY_7, handle_keymap_setworkspace, { .i32 = 7 } },\
    { MOD4, XKB_KEY_8, handle_keymap_setworkspace, { .i32 = 8 } },\
    { MOD4, XKB_KEY_9, handle_keymap_setworkspace, { .i32 = 9 } },\
    { MOD4|SHIFT, XKB_KEY_0, handle_keymap_movetoworkspace, { .i32 = 0 } },\
    { MOD4|SHIFT, XKB_KEY_1, handle_keymap_movetoworkspace, { .i32 = 1 } },\
    { MOD4|SHIFT, XKB_KEY_2, handle_keymap_movetoworkspace, { .i32 = 2 } },\
    { MOD4|SHIFT, XKB_KEY_3, handle_keymap_movetoworkspace, { .i32 = 3 } },\
    { MOD4|SHIFT, XKB_KEY_4, handle_keymap_movetoworkspace, { .i32 = 4 } },\
    { MOD4|SHIFT, XKB_KEY_5, handle_keymap_movetoworkspace, { .i32 = 5 } },\
    { MOD4|SHIFT, XKB_KEY_6, handle_keymap_movetoworkspace, { .i32 = 6 } },\
    { MOD4|SHIFT, XKB_KEY_7, handle_keymap_movetoworkspace, { .i32 = 7 } },\
    { MOD4|SHIFT, XKB_KEY_8, handle_keymap_movetoworkspace, { .i32 = 8 } },\
    { MOD4|SHIFT, XKB_KEY_9, handle_keymap_movetoworkspace, { .i32 = 9 } },

/* Misc */
#define MAX_CLIENTS 0xffff
#define NUM_WORKSPACES 10
#define BORDER_WIDTH 1

#endif /* CONFIG_H */
