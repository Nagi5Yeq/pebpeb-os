/**
 * @file keylayout.c
 * @author Andrew Spaulding (aspauldi)
 *
 * Definition for the active keyboard layout, which can be changed using
 * various macros.
 *
 * The placement of characters in the layout tables is bound to how scancodes
 * are interpreted on a qwerty keyboard. Any alternative layouts must rebind
 * keys with respect to this.
 *
 * Note that a zero in the layer table is not interpreted as 0 (or '\0'), but
 * instead informs the layer preprocessing logic that it needs to return the
 * empty value for that layer. This is done to ease the initialization of the
 * tables, as it lets us automatically set unlisted values to the layers empty
 * value. A literal 0 can be added to a table with KLE_ZERO.
 */

#include <x86/keyhelp.h>
#include <x86/keylayout.h>
#include <assert.h>

/** Defines the list of keyboard layouts supported by the system. */
static const unsigned char kb_layouts[KB_LAYOUT_COUNT][KL_COUNT][KL_SIZE] = {
  /** Qwerty keyboard layout (default). */
  [KB_LAYOUT_QWERTY] = {
    /* Base layer */
    { [0x01] = KHE_ESCAPE, '1', '2', '3', '4', '5', '6',
      '7', '8', '9', '0', '-', '=', '\b', '\t',
      'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
      'o', 'p', '[', ']', '\n', KHE_LCTL, 'a', 's',
      'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
      '\'', '`', KHE_LSHIFT, '\\', 'z', 'x', 'c', 'v',
      'b', 'n', 'm', ',', '.', '/', KHE_RSHIFT, '*',
      KHE_LALT, ' ', KHE_CAPSLOCK, KHE_F1, KHE_F2, KHE_F3, KHE_F4, KHE_F5,
      KHE_F6, KHE_F7, KHE_F8, KHE_F9, KHE_F10, KHE_NUMLOCK, KHE_UNDEFINED,
      KHE_HOME, KHE_ARROW_UP, KHE_PAGE_UP, '-',
      KHE_ARROW_LEFT, KHE_BEGIN, KHE_ARROW_RIGHT, '+',
      KHE_END, KHE_ARROW_DOWN, KHE_PAGE_DOWN, KHE_INSERT, KHE_DELETE,
      KHE_PRINT_SCREEN, /* Alt + Print */
      [0x57] = KHE_F11, KHE_F12, /* 0x59-0x7F */ },

    /* Lock layer */
    { /* Caps layer */
      [0x10] = 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
      [0x1E] = 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
      [0x2C] = 'Z', 'X', 'C', 'V', 'B', 'N', 'M',
      /* Num layer */
      [0x47] = '7', '8', '9',
      [0x4b] = '4', '5', '6',
      [0x4f] = '1', '2', '3', '0', '.' },

    /* Shift layer */
    { [0x02] = '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
      [0x1A] = '{', '}',
      [0x27] = ':', '\"', '~',
      [0x2B] = '|',
      [0x33] = '<', '>', '?' },

    /* Control layer */
    { [0x03] = KLE_ZERO, [0x07] = 0x1E, [0x0C] = 0x1F,
      [0x10] = 0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09,
      0x0F, 0x10, 0x1B, 0x1D,
      [0x1E] = 0x01, 0x13, 0x04, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      [0x2B] = 0x1C, 0x1A, 0x18, 0x03, 0x16, 0x02, 0x0E, 0x0D },

    /* Extended layer. Cannot be lowered from. */
    { [0x1C] = '\n', KHE_RCTL,
      [0x35] = '/', [0x37] = KHE_PRINT_SCREEN, KHE_RALT,
      [0x48] = KHE_ARROW_UP, [0x4B] = KHE_ARROW_LEFT,
      [0x4D] = KHE_ARROW_RIGHT, [0x50] = KHE_ARROW_DOWN,
      [0x53] = KHE_DELETE, [0x5D] = KHE_APPS }
  },

  /** Dvorak keyboard layout. */
  [KB_LAYOUT_DVORAK] = {
    /* Base layer */
    { [0x01] = KHE_ESCAPE, '1', '2', '3', '4', '5', '6',
      '7', '8', '9', '0', '[', ']', '\b', '\t',
      '\'', ',', '.', 'p', 'y', 'f', 'g', 'c',
      'r', 'l', '/', '=', '\n', KHE_LCTL, 'a', 'o',
      'e', 'u', 'i', 'd', 'h', 't', 'n', 's',
      '-', '`', KHE_LSHIFT, '\\', ';', 'q', 'j', 'k',
      'x', 'b', 'm', 'w', 'v', 'z', KHE_RSHIFT, '*',
      KHE_LALT, ' ', KHE_CAPSLOCK, KHE_F1, KHE_F2, KHE_F3, KHE_F4, KHE_F5,
      KHE_F6, KHE_F7, KHE_F8, KHE_F9, KHE_F10, KHE_NUMLOCK, KHE_UNDEFINED,
      KHE_HOME, KHE_ARROW_UP, KHE_PAGE_UP, '-',
      KHE_ARROW_LEFT, KHE_BEGIN, KHE_ARROW_RIGHT, '+',
      KHE_END, KHE_ARROW_DOWN, KHE_PAGE_DOWN, KHE_INSERT, KHE_DELETE,
      KHE_PRINT_SCREEN, /* Alt + Print */
      [0x57] = KHE_F11, KHE_F12, /* 0x59-0x7F */ },

    /* Lock layer */
    { /* Caps layer */
      [0x13] = 'P', 'Y', 'F', 'G', 'C', 'R', 'L',
      [0x1E] = 'A', 'O', 'E', 'U', 'I', 'D', 'H', 'T', 'N', 'S',
      [0x2D] = 'Q', 'J', 'K', 'X', 'B', 'M', 'W', 'V', 'Z',
      /* Num layer */
      [0x47] = '7', '8', '9',
      [0x4b] = '4', '5', '6',
      [0x4f] = '1', '2', '3', '0', '.' },

    /* Shift layer */
    { [0x02] = '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '{', '}',
      [0x10] = '\"', '<', '>',
      [0x1A] = '?', '+',
      [0x28] = '_', '~', [0x2B] = '|', ':' },

    /* Control layer */
    { /** @bug Still bound to qwerty. */
      [0x03] = KLE_ZERO, [0x07] = 0x1E, [0x0C] = 0x1F,
      [0x10] = 0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09,
      0x0F, 0x10, 0x1B, 0x1D,
      [0x1E] = 0x01, 0x13, 0x04, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      [0x2B] = 0x1C, 0x1A, 0x18, 0x03, 0x16, 0x02, 0x0E, 0x0D },

    /* Extended layer. Cannot be lowered from. */
    { [0x1C] = '\n', KHE_RCTL,
      [0x35] = '/', [0x37] = KHE_PRINT_SCREEN, KHE_RALT,
      [0x48] = KHE_ARROW_UP, [0x4B] = KHE_ARROW_LEFT,
      [0x4D] = KHE_ARROW_RIGHT, [0x50] = KHE_ARROW_DOWN,
      [0x53] = KHE_DELETE, [0x5D] = KHE_APPS }
  },

  /** Colemak keyboard layout. */
  [KB_LAYOUT_COLEMAK] = {
    /* Base layer */
    { [0x01] = KHE_ESCAPE, '1', '2', '3', '4', '5', '6',
      '7', '8', '9', '0', '-', '=', '\b', '\t',
      'q', 'w', 'f', 'p', 'g', 'j', 'l', 'u',
      'y', ';', '[', ']', '\n', KHE_LCTL, 'a', 'r',
      's', 't', 'd', 'h', 'n', 'e', 'i', 'o',
      '\'', '`', KHE_LSHIFT, '\\', 'z', 'x', 'c', 'v',
      'b', 'k', 'm', ',', '.', '/', KHE_RSHIFT, '*',
      KHE_LALT, ' ', KHE_CAPSLOCK, KHE_F1, KHE_F2, KHE_F3, KHE_F4, KHE_F5,
      KHE_F6, KHE_F7, KHE_F8, KHE_F9, KHE_F10, KHE_NUMLOCK, KHE_UNDEFINED,
      KHE_HOME, KHE_ARROW_UP, KHE_PAGE_UP, '-',
      KHE_ARROW_LEFT, KHE_BEGIN, KHE_ARROW_RIGHT, '+',
      KHE_END, KHE_ARROW_DOWN, KHE_PAGE_DOWN, KHE_INSERT, KHE_DELETE,
      KHE_PRINT_SCREEN, /* Alt + Print */
      [0x57] = KHE_F11, KHE_F12, /* 0x59-0x7F */ },

    /* Lock layer */
    { /* Caps layer */
      [0x10] = 'Q', 'W', 'F', 'P', 'G', 'J', 'L', 'U', 'Y',
      [0x1E] = 'A', 'R', 'S', 'T', 'D', 'H', 'N', 'E', 'I', 'O',
      [0x2C] = 'Z', 'X', 'C', 'V', 'B', 'K', 'M',
      /* Num layer */
      [0x47] = '7', '8', '9',
      [0x4b] = '4', '5', '6',
      [0x4f] = '1', '2', '3', '0', '.' },

    /* Shift layer */
    { [0x02] = '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
      [0x19] = ':', '{', '}',
      [0x28] = '\"', '~',
      [0x2B] = '|',
      [0x33] = '<', '>', '?' },

    /* Control layer */
    { /** @bug Still bound to qwerty. */
      [0x03] = KLE_ZERO, [0x07] = 0x1E, [0x0C] = 0x1F,
      [0x10] = 0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09,
      0x0F, 0x10, 0x1B, 0x1D,
      [0x1E] = 0x01, 0x13, 0x04, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      [0x2B] = 0x1C, 0x1A, 0x18, 0x03, 0x16, 0x02, 0x0E, 0x0D },

    /* Extended layer. Cannot be lowered from. */
    { [0x1C] = '\n', KHE_RCTL,
      [0x35] = '/', [0x37] = KHE_PRINT_SCREEN, KHE_RALT,
      [0x48] = KHE_ARROW_UP, [0x4B] = KHE_ARROW_LEFT,
      [0x4D] = KHE_ARROW_RIGHT, [0x50] = KHE_ARROW_DOWN,
      [0x53] = KHE_DELETE, [0x5D] = KHE_APPS }
  }
};

/**
 * Defines the value that layer processing should return when it finds a
 * zero in the layer table.
 */
static unsigned char kb_empty[KL_COUNT] = {
    [KL_BASE] = KHE_UNDEFINED,
    [KL_LOCK] = KLE_LOWER,
    [KL_SHIFT] = KLE_LOWER,
    [KL_CTL] = KLE_LOWER,
    [KL_EXT] = KHE_UNDEFINED
};

/** Holds the systems active keyboard layout. Qwerty by default. */
static kb_layout_t active_layout = KB_LAYOUT_QWERTY;

/**
 * @brief Reads a key from the active keyboard layout.
 *
 * Attempts to read the keycode from the given layer of the current layout.
 *
 * It is illegal to give an invalid layer or out of range keycode.
 *
 * @param layer The layer to read the key from.
 * @param keycode The keycode of the key to be obtained.
 * @return A character representative of the key and layout, or KHE_UNDEFINED.
 */
unsigned char
kl_get_key(kl_layer_t layer, int keycode)
{
  reject((layer < 0) || (layer >= KL_COUNT));
  reject((keycode < 0) || (keycode >= KL_SIZE));

  affirm_msg((KB_LAYOUT_MIN <= active_layout) &&
             (active_layout < KB_LAYOUT_COUNT),
             "kl_get_key() found a corrupt active keyboard layout!");

  // Get the key and perform pre-processing on it.
  unsigned char key = kb_layouts[active_layout][layer][keycode];
  key = (key == KLE_EMPTY) ? kb_empty[layer] : key;
  key = (key == KLE_ZERO) ? 0x00 : key;
  return key;
}

/**
 * @brief Sets the active keyboard layout to the given layout.
 *
 * It is illegal to give this function an invalid layout.
 *
 * @param layout The layout to be set as the active layout.
 * @return void
 */
void
kl_set_layout(kb_layout_t layout)
{
  reject((layout < KB_LAYOUT_MIN) || (layout >= KB_LAYOUT_COUNT));

  active_layout = layout;
}
