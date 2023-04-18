/** @file dog.c
 *
 *  @brief PebPeb port of P1 410_test.c
 *
 *  @author cabauer
 *  @author jrduvall
 *  @author de0u
 *
 *  PebPeb-only executable -- it doesn't work on hardware.
 *
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <common_kern.h>

#include <simics.h>            /* lprintf() */
#include <multiboot.h>         /* boot_info */

#include <console.h>           /* putbyte() */
#include <hvcall.h>
#include <devices.h>           /* augchar(), kbd_intr(), timer_intr() */
#include <x86/keyhelp.h>       /* KH_* */
#include <x86/video_defines.h> /* colors */

/* --- Stuff and Nonsense --- */

void wait_char(char testc);
void cls(void);

/**
 * @brief Wait for a specific character to be pressed
 *
 * @param testc the character to test for
 *
 * @requires interrupts are disabled
 * @ensures interrupts are disabled
 * @ensures returns only once the given character is typed
 */
void wait_char(char testc) {
  kh_type a;
  do {
    a = augchar();
    lprintf("augchar()=%d hasdata=%d getchar=%d", a, KH_HASDATA(a),
            KH_GETCHAR(a));
  } while (!KH_HASDATA(a) || KH_GETCHAR(a) != testc);
}

/** @brief Clear screen */
void cls(void) {
  char row[CONSOLE_WIDTH];

  memset(row, ' ', sizeof (row));

  hv_cons_set_cursor_pos(0, 0);
  for (int r = 0; r < CONSOLE_HEIGHT; ++r)
    putbytes(row, CONSOLE_WIDTH);
  hv_cons_set_cursor_pos(0, 0);
}

/* --- Kernel entrypoint --- */

/** @brief Kernel entrypoint.
 *
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t* mbinfo, int argc, char** argv, char** envp) {
  int ticks;

  if (!hv_isguest()) {
    lprintf("@@@@@ FAIL @@@@@ I am NOT supposed to be here!!!");
    panic("This payload is guest-only");
    return (-1);
  }

  unsigned int magic = hv_magic();
  if (magic != HV_MAGIC) {
    lprintf("@@@@@ FAIL @@@@@ Magic Failed");
    hv_exit(-10);
  }

  hv_setidt(HV_TICKBACK, (void*)timer_intr, HV_SETIDT_PRIVILEGED);
  hv_setidt(HV_KEYBOARD, (void*)kbd_intr, HV_SETIDT_PRIVILEGED);

  hv_cons_set_term_color(FGND_GREEN | BGND_BLACK);
  cls();

  hv_cons_set_cursor_pos(12, 34);
  printf("Hello World!\n");
  hv_cons_set_cursor_pos(15, 34);
  printf("Type \"dog\" now: ");

  lprintf("waiting for characters...");
  wait_char('d'); putbyte('c');
  wait_char('o'); putbyte('a');
  wait_char('g'); putbyte('t');
  hv_cons_set_cursor_pos(17, 34);
  printf("Yay kitties!\n");

  ticks = tick_count();
  lprintf("Ticks: %d", ticks);
  hv_cons_set_cursor_pos(18, 34);
  printf("Ticks: %d\n", ticks);

  hv_exit(0);
}
