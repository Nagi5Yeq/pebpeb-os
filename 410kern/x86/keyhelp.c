/**
 * The 15-410 reference kernel keyboard handling code.
 *
 * @author Steve Muckle <smuckle@andrew.cmu.edu>
 *
 * @author Edited by zra for the 2003-2004 season.
 *
 * @author Edited by mpa for spring 2004
 *
 * @author Rewritten by nwf for spring 2007.
 *
 * @author Rewritten by aspauldi for spring 2022.
 *
 * Functions for turning keyboard scancodes
 * into chars.
 *
 * Notice that we use Scancode Set 1
 */
/*@{*/

#include <x86/keyhelp.h>
#include <x86/keylayout.h>

#include <stddef.h>
#include <assert.h>

/** Deconstructs a scancode into its relevant parts */
/*@{*/
#define SCANCODE_GET_PRESS(s) (!(s & 0x80))
#define SCANCODE_GET_CODE(s) (s & 0x7F)
/*@}*/

/**
 * Defines the current state of scancode processing.
 *
 * Most keys will be processed in the simple or extended states, including
 * KHE_PRINT. In some situations, KHE_CTL_PAUSE and KHE_PAUSE will
 * need to process longer scancodes. In such situations, key_sequence is used
 * to determine which sub-state of PAUSE/CTL+PAUSE is being used.
 */
typedef enum {
  KS_SIMPLE,
  KS_EXT,
  KS_PAUSE,
  KS_CTL_PAUSE
} kb_state_t;

/**
 * This is returned as the upper bits of the result of
 * process_scancode and may be interrogated more readily by
 * the KH_ macros in keyhelp.h
 *
 * WARNING:
 * The bottom bits overlap with the KH_RESULT_ codes in the
 * return value.
 */
static short key_state = 0;

/**
 * Tracks which kind of key is currently being processed, and which substate
 * of that processing the key is in. key_sequence is only relevent to the
 * KS_PAUSE and KS_CTL_PAUSE states.
 */
/*@{*/
static kb_state_t scan_state = KS_SIMPLE;
static int key_sequence = 0;
/*@}*/

/**
 * Checks if the given scancode came from the numpad.
 *
 * @param scancode a simple scancode
 * @return True if the code came from the numpad, false otherwise.
 */
static int
is_numpad_scan(int scancode)
{
  int res = 0;

  switch(scan_state)
  {
    case KS_SIMPLE:
      res = (scancode == 0x37) || ((0x47 <= scancode) && (scancode <= 0x53));
      break;
    case KS_EXT:
      res = (scancode == 0x1C) || (scancode == 0x35);
      break;
    case KS_PAUSE:
    case KS_CTL_PAUSE:
      break;
    default:
      panic("Illegal keyboard scancode state in is_numpad_scan()");
  }

  return res;
}

/**
 * Converts the given information into a final kh_type, ready to be returned to
 * the caller of process_scancode.
 *
 * Note that this function always considers the raw code to be present,
 * regardless of its value. Intermediate states should instead return the
 * current modifier state directly.
 *
 * This function must be called after the modifier key state has been updated.
 * Additionally, this processing must happen before scan_state tranitions,
 * or else the \n and / keys on the numpad may not be flagged as a numpad
 * key correctly.
 *
 * @param pressed Set when the key is being pressed.
 * @param keycode A simple scancode.
 * @param code The processed character result.
 * @param rcode The base character result.
 * @return The final processed kh_type.
 */
static kh_type
process_result(int pressed, int keycode,
               unsigned char code, unsigned char rcode)
{
  kh_type res = 0;

  reject(SCANCODE_GET_CODE(keycode) != keycode);

  /* Check if code exists. */
  if(rcode != KHE_UNDEFINED && code != KHE_UNDEFINED)
    res |= (KH_RESULT_HASDATA << KH_RMODS_SHIFT);
  else
    code = 0;

  if(pressed)
    res |= KH_RESULT_MAKE << KH_RMODS_SHIFT;

  if(is_numpad_scan(keycode))
    res |= KH_RESULT_NUMPAD << KH_RMODS_SHIFT;

  return res | ((code & KH_CHAR_SMASK) << KH_CHAR_SHIFT)
             | ((rcode & KH_RAWCHAR_SMASK) << KH_RAWCHAR_SHIFT)
             | (KH_RESULT_HASRAW << KH_RMODS_SHIFT)
             | ((key_state & KH_STATE_SMASK) << KH_STATE_SHIFT);
}

/**
 * Uses the given code and rcode to update the modifier key state. Updates
 * the code, if necessary.
 *
 * @param code The code to be processed and updated.
 * @param rcode The raw code to be processed.
 * @param pressed Set when the key event that generated the rcode was a make.
 */
static void
process_mod_key(unsigned char *code, unsigned char rcode, int pressed)
{
  int mod_key;

  reject(code == NULL);

  switch(rcode)
  {
    case KHE_LCTL:
      mod_key = KH_LCONTROL_KEY;
      break;
    case KHE_RCTL:
      mod_key = KH_RCONTROL_KEY;
      break;
    case KHE_LSHIFT:
      mod_key = KH_LSHIFT_KEY;
      break;
    case KHE_RSHIFT:
      mod_key = KH_RSHIFT_KEY;
      break;
    case KHE_LALT:
      mod_key = KH_LALT_KEY;
      break;
    case KHE_RALT:
      mod_key = KH_RALT_KEY;
      break;
    case KHE_CAPSLOCK:
      mod_key = KH_CAPS_LOCK;
      break;
    case KHE_NUMLOCK:
      mod_key = KH_NUM_LOCK;
      break;
    default:
      mod_key = -1;
      break;
  }

  if(mod_key > 0)
  {
    /* compat */
    *code = KHE_UNDEFINED;

    if((mod_key == KH_CAPS_LOCK) || (mod_key == KH_NUM_LOCK)) /* toggle */
      key_state = (pressed) ? key_state ^ mod_key : key_state;
    else /* absolute */
      key_state = (pressed) ? key_state | mod_key : key_state & ~mod_key;
  }
}

/**
 * Determines which layer the processed code should come from.
 *
 * @param keycode a simple scancode.
 * @return The layer which should be used to process the scancode.
 */
static kl_layer_t
process_simple_layer(int keycode)
{
  int shift_pressed = !!(key_state & (KH_LSHIFT_KEY | KH_RSHIFT_KEY));
  int caps_locked = !!(key_state & KH_CAPS_LOCK);
  int num_locked = !!(key_state & KH_NUM_LOCK);
  int ctl_pressed = !!(key_state & (KH_LCONTROL_KEY | KH_RCONTROL_KEY));
  int is_numpad = is_numpad_scan(keycode);
  kl_layer_t layer = KL_BASE;

  reject(SCANCODE_GET_CODE(keycode) != keycode);

  if (ctl_pressed && (kl_get_key(KL_CTL, keycode) != KLE_LOWER)) {
    layer = KL_CTL;
  } else if (shift_pressed && (kl_get_key(KL_SHIFT, keycode) != KLE_LOWER)) {
    layer = KL_SHIFT;
  } else if ((((caps_locked != shift_pressed) && !is_numpad) ||
               (num_locked && is_numpad)) &&
             (kl_get_key(KL_LOCK, keycode) != KLE_LOWER)) {
    layer = KL_LOCK;
  }

  return layer;
}

/**
 * Processes scancodes in the simple state by either transitioning to the
 * extended state or mapping the simple scancode to a character.
 *
 * @param keypress The scancode to be processed.
 * @return The final kh_type to be returned by process_scancode.
 */
static kh_type
process_simple_scan(int keypress)
{
  int pressed = SCANCODE_GET_PRESS(keypress);
  int keycode = SCANCODE_GET_CODE(keypress);
  unsigned char code = KHE_UNDEFINED;
  unsigned char rcode = KHE_UNDEFINED;
  kl_layer_t code_layer = KL_BASE;
  kh_type res = (key_state & KH_STATE_SMASK) << KH_STATE_SHIFT;

  switch(keypress)
  {
    case 0xE0:
      /* Begin extended sequence */
      scan_state = KS_EXT;
      break;
    case 0xE1:
      /* Begin pause sequence */
      scan_state = KS_PAUSE;
      key_sequence = 0;
      break;
    default:
      /* Determine the layer and process the layout. */
      code_layer = process_simple_layer(keycode);
      rcode = kl_get_key(KL_BASE, keycode);
      code = kl_get_key(code_layer, keycode);

      /* Perform final processing on the result */
      process_mod_key(&code, rcode, pressed);
      res = process_result(pressed, keycode, code, rcode);
      break;
  }

  return res;
}

/**
 * Processes scancodes in the extended state.  Notably, this includes
 * the arrow keys as well as some of the more unusual keys
 * on the keyboard.
 *
 * @param keypress The scancode to be processed.
 * @return A fully processed kh_type result.
 */
static kh_type
process_extended_scan(int keypress)
{
  int pressed = SCANCODE_GET_PRESS(keypress);
  int keycode = SCANCODE_GET_CODE(keypress);
  unsigned char code = KHE_UNDEFINED;
  unsigned char rcode = KHE_UNDEFINED;
  kh_type res = (key_state & KH_STATE_SMASK) << KH_STATE_SHIFT;

  switch(keypress)
  {
    case 0x2A: /* Press */
    case 0xAA: /* Release */
      /* Fake LSHIFT (used by print and numpad keys) - intermediate state. */
      scan_state = KS_SIMPLE;
      break;
    case 0x46:
      /* Control + Pause sequence start */
      scan_state = KS_CTL_PAUSE;
      key_sequence = 0;
      break;
    default:
      /* Get key and process. Must process_result() before state transition. */
      rcode = code = kl_get_key(KL_EXT, keycode);
      process_mod_key(&code, rcode, pressed);
      res = process_result(pressed, keycode, code, rcode);
      scan_state = KS_SIMPLE;
      break;
  }

  return res;
}

/**
 * Processes scancodes in the pause states (pause/ctl+pause).
 *
 * Note that the original implementation of keyhelp did something undefined
 * if a sequence was interrupted (it either pretended that the scancode
 * wasn't interrupting the sequence or reset the state). As such, this
 * implementation simply resets the state to KS_SIMPLE when a sequence is
 * interrupted.
 *
 * @param keypress The scancode to be processed.
 * @return A fully precessed kh_type.
 */
static kh_type
process_pause_scan(int keypress)
{
  /* Determine which sequence should be checked. */
  const unsigned char pause_seq[] = { 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
  const unsigned char ctl_pause_seq[] = { 0xE0, 0xC6 };
  const unsigned char *seq = (scan_state == KS_PAUSE)
                           ? pause_seq : ctl_pause_seq;
  unsigned int seq_size = (scan_state == KS_PAUSE)
                        ? sizeof(pause_seq) : sizeof(ctl_pause_seq);

  int keycode = SCANCODE_GET_CODE(keypress);
  kh_type res = (key_state & KH_STATE_SMASK) << KH_STATE_SHIFT;

  affirm((scan_state == KS_PAUSE) || (scan_state == KS_CTL_PAUSE));
  affirm((0 <= key_sequence) && (key_sequence < seq_size));

  if(seq[key_sequence] == keypress)
  {
    key_sequence++;
    if(key_sequence == seq_size)
    {
      /* Only makes are reported for pause. */
      res = process_result(1, keycode, KHE_PAUSE, KHE_PAUSE);
      scan_state = KS_SIMPLE;
    }
  } else {
    /* No match, reset. */
    scan_state = KS_SIMPLE;
  }

  return res;
}

/** The entrypoint to the keyboard processing library.
 *
 * @param keypress A raw scancode as returned by the keyboard hardware.
 * @return A kh_type indicating the keyboard modifier key states, result
 *         modifier bits, and potentially ASCII/410 Upper Code Plane
 *         translations.
 */
kh_type
process_scancode(int keypress)
{
  kh_type res;

  switch(scan_state)
  {
    case KS_SIMPLE:
      res = process_simple_scan(keypress);
      break;
    case KS_EXT:
      res = process_extended_scan(keypress);
      break;
    case KS_PAUSE:
    case KS_CTL_PAUSE:
      res = process_pause_scan(keypress);
      break;
    default:
      panic("Illegal keyboard scancode state in process_scancode()");
  }

  /*
   * We must return at least the modifier bits.
   *
   * key_state needs to be cast to int without sign extension for this check
   * to work (integer promotion, grr...), which is why the mask is applied.
   */
  affirm(KH_STATE(res) == (key_state & KH_STATE_SMASK));

  return res;
}

/*@}*/
