/** @file console.c
 *
 *  @brief Exercises the hypervisor console
 *
 *  @author relong
 *  @author de0u
 *
 *  PebPeb-only executable -- it doesn't work on hardware.
 *
 */

#include <common_kern.h>

/* libc includes. */
#include <malloc.h>
#include <simics.h> /* lprintf() */
#include <stdio.h>

/* multiboot header file */
#include <multiboot.h> /* boot_info */

/* memory includes. */
#include <lmm.h> /* lmm_remove_free() */

/* x86 specific includes */
#include <x86/asm.h> /* enable_interrupts() */
#include <x86/cr.h>
#include <x86/interrupt_defines.h> /* interrupt_setup() */
#include <x86/page.h>
#include <x86/seg.h> /* install_user_segs() */

#include <hvcall.h>
#include <malloc_internal.h>
#include <video_defines.h>

#define DEBUG
#include <contracts.h>

#include <assert.h>
#include <string.h>

static const int TOP_ROW = 2;
static const int BOT_ROW = 22;

static const int MAGIC_ROW = 5;
static const int MAGIC_COL = 10;

#define STRLEN(s) (sizeof(s) - 1)
static const char BLANKS[]   = "                                                            ";
static const char STARS[]    = "************************************************************";
static const char IN_HV[]    = "We are in the hypervisor\n";
static const char COLORS[]   = "Look at the pretty colors!\n";
static const char PASSING[]  = "All tests passing\n";
static const char TEST[]     = "This is a test";
static const char CRACKERS[] = "It's crackers to slip a rozzer the dropsy in snide.\n";
static const char EXITING[]  = "About to: hv_exit(77)...\n";

/** @brief Kernel entrypoint.
 *
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t *mbinfo, int argc, char **argv, char **envp) {

    /* We do NOT want to print that tests passed if they didn't.
     * Some hypervisors might botch hv_exit() -- it might return!
     * Note that hv_exit() is declared as NORETURN.
     * So GCC will **DELETE** a call to panic() after hv_exit().
     * Thanks, GCC hyper-optimizers!
     */
    int pass = 1;

    if (!hv_isguest()) {
        pass = 0;
        lprintf("@@@@@ FAIL @@@@@ I am NOT supposed to be here!!!");
        panic("This payload is guest-only");
        return (-1);
    }

    lprintf(STARS);
    lprintf("In guest kernel");

    int magic = hv_magic();
    lprintf("Found magic: %d", magic);
    if (magic != HV_MAGIC) {
        pass = 0;
        lprintf("@@@@@ FAIL @@@@@ Magic Failed");
        hv_exit(-10);
    }


    lprintf("Blank area");
    hv_cons_set_cursor_pos(TOP_ROW, 0);
    hv_cons_set_term_color(FGND_RED | BGND_BLACK);
    hv_print(STRLEN(STARS), (unsigned char *)STARS);
    hv_print(1, (unsigned char *)"\n");
    hv_cons_set_term_color(FGND_WHITE | BGND_BLACK);
    for (int row = TOP_ROW+1; row < BOT_ROW-1; ++row) {
        hv_print(STRLEN(BLANKS), (unsigned char *)BLANKS);
        hv_print(1, (unsigned char *)"\n");
    }
    hv_cons_set_term_color(FGND_RED | BGND_BLACK);
    hv_print(STRLEN(STARS), (unsigned char *)STARS);
    hv_print(1, (unsigned char *)"\n");
    hv_cons_set_term_color(FGND_WHITE | BGND_BLACK);
    hv_cons_set_cursor_pos(TOP_ROW+1, 0);

    lprintf("Trying to print to the hypervisor");
    hv_print(STRLEN(IN_HV), (unsigned char *)IN_HV);

    lprintf("Color printing");
    hv_cons_set_term_color(FGND_PINK | BGND_LGRAY);
    hv_print(STRLEN(COLORS), (unsigned char *)COLORS);

    lprintf("Moving cursor to (%d,%d)", MAGIC_ROW, MAGIC_COL);
    hv_cons_set_cursor_pos(MAGIC_ROW, MAGIC_COL);

    lprintf("Trying to get the cursor information now");
    int row, col;
    hv_cons_get_cursor_pos(&row, &col);

    lprintf("Found row, col: (%d,%d)", row, col);

    if (row != MAGIC_ROW || col != MAGIC_COL) {
        pass = 0;
        lprintf("@@@@@ FAIL @@@@@ Bad cursor position: (%d,%d)", row, col);
        hv_exit(-11);
    }
    hv_cons_set_term_color(FGND_WHITE | BGND_BLACK);

    hv_print_at(STRLEN(TEST),
                (unsigned char *)TEST,
                MAGIC_ROW + 1,  // Add some extra magic
                2 * MAGIC_COL,  // Add some extra magic
                FGND_CYAN | BGND_LGRAY);

    int new_row, new_col;
    hv_cons_get_cursor_pos(&new_row, &new_col);

    if (new_row != row || new_col != col) {
        pass = 0;
        lprintf("@@@@@ FAIL @@@@@ hv_print_at() changes state!!!");
        hv_exit(-12);
    }

    lprintf("Verifying movement");
    hv_cons_set_cursor_pos(MAGIC_ROW+5, 0);
    hv_cons_set_term_color(FGND_GREEN | BGND_BLACK);
    hv_print(STRLEN(CRACKERS), (unsigned char *)CRACKERS);
    hv_cons_set_term_color(FGND_WHITE | BGND_BLACK);
    hv_cons_get_cursor_pos(&new_row, &new_col);
    if (new_row != MAGIC_ROW+6 || new_col != 0) {
        pass = 0;
        lprintf("@@@@@ FAIL @@@@@ hv_print() does not move cursor correctly!!!");
        hv_exit(-13);
    }
    hv_print(2, (unsigned char *)"\n\n");

    if (pass) {
        lprintf("ALL TESTS PASSED");
        hv_print(STRLEN(PASSING), (unsigned char *)PASSING);
    }

    lprintf("About to: hv_exit(77)...");
    hv_print(STRLEN(EXITING), (unsigned char *)EXITING);

    lprintf(STARS);
    hv_print(1, (unsigned char *)"\n");
    hv_exit(77);

    panic("Exit failed?");
}
