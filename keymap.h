#define keymap_key_max 0xff
#define keymap_fn_key_scan 0xdb
const int keymap_direct[] = {
	[0x01] = KEY_ESC,
	[0x02] = KEY_1,
	[0x03] = KEY_2,
	[0x04] = KEY_3,
	[0x05] = KEY_4,
	[0x06] = KEY_5,
	[0x07] = KEY_6,
	[0x08] = KEY_7,
	[0x09] = KEY_8,
	[0x0a] = KEY_9,
	[0x0b] = KEY_0,
	[0x0c] = KEY_MINUS,
	[0x0d] = KEY_EQUAL,
	[0x0e] = KEY_BACKSPACE,
	[0x0f] = KEY_TAB,
	[0x10] = KEY_Q,
	[0x11] = KEY_W,
	[0x12] = KEY_E,
	[0x13] = KEY_R,
	[0x14] = KEY_T,
	[0x15] = KEY_Y,
	[0x16] = KEY_U,
	[0x17] = KEY_I,
	[0x18] = KEY_O,
	[0x19] = KEY_P,
	[0x1a] = KEY_LEFTBRACE,
	[0x1b] = KEY_RIGHTBRACE,
	[0x1c] = KEY_ENTER,
	[0x1d] = KEY_LEFTCTRL,
	[0x1e] = KEY_A,
	[0x1f] = KEY_S,
	[0x20] = KEY_D,
	[0x21] = KEY_F,
	[0x22] = KEY_G,
	[0x23] = KEY_H,
	[0x24] = KEY_J,
	[0x25] = KEY_K,
	[0x26] = KEY_L,
	[0x27] = KEY_SEMICOLON,
	[0x28] = KEY_APOSTROPHE,
	[0x29] = KEY_GRAVE,
	[0x2a] = KEY_LEFTSHIFT,
	[0x2b] = KEY_BACKSLASH,
	[0x2c] = KEY_Z,
	[0x2d] = KEY_X,
	[0x2e] = KEY_C,
	[0x2f] = KEY_V,
	[0x30] = KEY_B,
	[0x31] = KEY_N,
	[0x32] = KEY_M,
	[0x33] = KEY_COMMA,
	[0x34] = KEY_DOT,
	[0x35] = KEY_SLASH,
	[0x36] = KEY_RIGHTSHIFT,
	[0x38] = KEY_LEFTALT,
	[0x39] = KEY_SPACE,
	[0x3b] = KEY_BACK, /* back keys */
	[0x3c] = KEY_REFRESH, /* refresh key */
	[0x3d] = KEY_F11, /* fullscreen key */
	[0x3e] = KEY_LEFTMETA, /* overview key */
	[0x3f] = KEY_BRIGHTNESSDOWN,
	[0x40] = KEY_BRIGHTNESSUP,
	[0x41] = KEY_PLAY,
	[0x42] = KEY_MUTE,
	[0x43] = KEY_VOLUMEDOWN,
	[0x44] = KEY_VOLUMEUP,
	[0x5d] = KEY_COMPOSE, /* Hamburger key */
	[0x9d] = KEY_RIGHTCTRL,
	[0xae] = KEY_VOLUMEDOWN,
	[0xb0] = KEY_VOLUMEUP,
	[0xb8] = KEY_RIGHTALT,
	[0xc8] = KEY_UP,
	[0xcb] = KEY_LEFT,
	[0xcd] = KEY_RIGHT,
	[0xd0] = KEY_DOWN,
	[0xd8] = KEY_LEFTMETA, /* Assisstant key */
	[0xdb] = KEY_LEFTMETA /* Search key */
};
const int keymap_fn[] = {
	[0x02] = KEY_F1, /* 1 */
	[0x03] = KEY_F2, /* 2 */
	[0x04] = KEY_F3, /* 3 */
	[0x05] = KEY_F4, /* 4 */
	[0x06] = KEY_F5, /* 5 */
	[0x07] = KEY_F6, /* 6 */
	[0x08] = KEY_F7, /* 7 */
	[0x09] = KEY_F8, /* 8 */
	[0x0a] = KEY_F9, /* 9 */
	[0x0b] = KEY_F10, /* 0 */
	[0x0c] = KEY_F11, /* - */
	[0x0d] = KEY_F12, /* = */
	[0x0e] = KEY_DELETE, /* backspace */
	[0x0f] = KEY_CAPSLOCK, /* tab */
	[0x23] = -1, /* h, first combo */
	[0x24] = -2, /* j */
	[0x25] = -3, /* k */
	[0x26] = -4, /* l */

	[0x3b] = -5, /* back keys */
	[0x3c] = -6, /* refresh key */
	[0x3d] = -7, /* fullscreen key */
	[0x3e] = -8, /* overview key */
	[0x3f] = -9,
	[0x40] = -10,
	[0x41] = -11,
	[0x42] = -12,
	[0x43] = -13,
	[0x44] = -14,
	[0x5d] = -15, /* Hamburger key */

	[0xc8] = KEY_PAGEUP, /* up */
	[0xcb] = KEY_HOME, /* left */
	[0xcd] = KEY_END, /* right */
	[0xd0] = KEY_PAGEDOWN, /* down */

#if 0
	[0x10] = KEY_Q,
	[0x11] = KEY_W,
	[0x12] = KEY_E,
	[0x13] = KEY_R,
	[0x14] = KEY_T,
	[0x15] = KEY_Y,
	[0x16] = KEY_U,
	[0x17] = KEY_I,
	[0x18] = KEY_O,
	[0x19] = KEY_P,
	[0x1a] = KEY_LEFTBRACE,
	[0x1b] = KEY_RIGHTBRACE,
	[0x1c] = KEY_ENTER,
	[0x1d] = KEY_LEFTCTRL,
	[0x1e] = KEY_A,
	[0x1f] = KEY_S,
	[0x20] = KEY_D,
	[0x21] = KEY_F,
	[0x22] = KEY_G,
	[0x27] = KEY_SEMICOLON,
	[0x28] = KEY_APOSTROPHE,
	[0x29] = KEY_GRAVE,
	[0x2a] = KEY_LEFTSHIFT,
	[0x2b] = KEY_BACKSLASH,
	[0x2c] = KEY_Z,
	[0x2d] = KEY_X,
	[0x2e] = KEY_C,
	[0x2f] = KEY_V,
	[0x30] = KEY_B,
	[0x31] = KEY_N,
	[0x32] = KEY_M,
	[0x33] = KEY_COMMA,
	[0x34] = KEY_DOT,
	[0x35] = KEY_SLASH,
	[0x36] = KEY_RIGHTSHIFT,
	[0x38] = KEY_LEFTALT,
	[0x39] = KEY_SPACE,
	[0x9d] = KEY_RIGHTCTRL,
	[0xb8] = KEY_RIGHTALT,
	[0xd8] = KEY_LEFTALT,
	[0xdb] = KEY_SEARCH /* Search key */
#endif
};

const int keymap_combo_ctrl_alt_left[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFT, 0 };
const int keymap_combo_ctrl_alt_up[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_UP, 0 };
const int keymap_combo_ctrl_alt_down[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_DOWN, 0 };
const int keymap_combo_ctrl_alt_right[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_RIGHT, 0 };
const int keymap_combo_ctrl_alt_f1[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F1, 0 };
const int keymap_combo_ctrl_alt_f2[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F2, 0 };
const int keymap_combo_ctrl_alt_f3[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F3, 0 };
const int keymap_combo_ctrl_alt_f4[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F4, 0 };
const int keymap_combo_ctrl_alt_f5[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F5, 0 };
const int keymap_combo_ctrl_alt_f6[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F6, 0 };
const int keymap_combo_ctrl_alt_f7[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F7, 0 };
const int keymap_combo_ctrl_alt_f8[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F8, 0 };
const int keymap_combo_ctrl_alt_f9[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F9, 0 };
const int keymap_combo_ctrl_alt_f10[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_F10, 0 };
const int keymap_combo_ctrl_alt_del[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_DELETE, 0 };

const int * const keymap_combo[] = {
	keymap_combo_ctrl_alt_left,
	keymap_combo_ctrl_alt_down,
	keymap_combo_ctrl_alt_up,
	keymap_combo_ctrl_alt_right,
	keymap_combo_ctrl_alt_f1,
	keymap_combo_ctrl_alt_f2,
	keymap_combo_ctrl_alt_f3,
	keymap_combo_ctrl_alt_f4,
	keymap_combo_ctrl_alt_f5,
	keymap_combo_ctrl_alt_f6,
	keymap_combo_ctrl_alt_f7,
	keymap_combo_ctrl_alt_f8,
	keymap_combo_ctrl_alt_f9,
	keymap_combo_ctrl_alt_f10,
	keymap_combo_ctrl_alt_del,
};
