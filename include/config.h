/* Include guard */
#ifndef CONFIG_H
#define CONFIG_H

/* Logging */
#define LOGS 1      /* Enable logging */
#define ANSI_LOGS 1 /* Enable formatted logs with ANSI escape codes */

/* Keymaps */
#define SHIFT XCB_MOD_MASK_SHIFT
#define LOCK XCB_MOD_MASK_LOCK
#define CONTROL XCB_MOD_MASK_CONTROL
#define MOD1 XCB_MOD_MASK_1
#define MOD2 XCB_MOD_MASK_2
#define MOD3 XCB_MOD_MASK_3
#define MOD4 XCB_MOD_MASK_4
#define MOD5 XCB_MOD_MASK_5
static const char *termcmd[] = { "st", (void *)(0) };
#define KEYMAPS \
    { MOD1|SHIFT, XKB_KEY_c, handle_keymap_quit, { .i32 = 0 } },\
    { MOD1, XKB_KEY_Return, handle_keymap_spawnprocess, { .ptr = termcmd } },

#endif /* CONFIG_H */
