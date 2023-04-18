/**
 * @file keylayout.h
 * @author Andrew Spaulding (aspauldi)
 * @brief Defines the interface and constants to be used to interract with
 *        and/or change the active keyboard layout.
 */

#ifndef __KEYLAYOUT_INTERNAL_H__
#define __KEYLAYOUT_INTERNAL_H__

/** The number of scancodes in a layer for a keyboard layout. */
#define KL_SIZE 128

/**
 * Defines the keyboard layouts supported by the system. The active keyboard
 * layout may be changed by call kl_set_layout().
 *
 * KB_LAYOUT_MIN and KB_LAYOUT_COUNT compare true to the minimum layout number
 * and total number of keyboard layouts respectively.
 */
typedef enum {
  KB_LAYOUT_MIN = 0,
  KB_LAYOUT_QWERTY = KB_LAYOUT_MIN,
  KB_LAYOUT_DVORAK,
  KB_LAYOUT_COLEMAK,
  KB_LAYOUT_COUNT
} kb_layout_t;

/**
 * Defines the layers which can be accessed in the scancode table for the
 * basic plain.
 *
 * Finding a KLE_LOWER in a layer means that the scancode decoding logic
 * will try to use the lower layer. The requirements for that layer must
 * still be satisfied, however. E.g., dropping from shift to lock won't use
 * numpad keys if num lock isn't on.
 */
typedef enum {
  KL_BASE = 0,
  KL_LOCK = 1,
  KL_SHIFT = 2,
  KL_CTL = 3,
  KL_EXT = 4,
  KL_COUNT
} kl_layer_t;

/**
 * Special values for keyboard layout layer tables.
 *
 * KLE_EMPTY is an alias for zero, it means the slot in the lookup table wasn't
 * specified. KLE_ZERO is used to ask kl_get_key to return 0. KLE_LOWER is used
 * to inform the layer processing logic that it should try the next layer down.
 */
enum {
    KLE_EMPTY = 0x00,
    KLE_ZERO = 0xFE,
    KLE_LOWER = 0xFF,
};

/* Reads a key from the given layer of the active layout */
unsigned char kl_get_key(kl_layer_t layer, int keycode);

/* Sets the active keyboard layout */
void kl_set_layout(kb_layout_t layout);

#endif /* __KEYLAYOUT_INTERNAL_H__ */
