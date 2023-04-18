/** @file warp_main.c
 *  @brief Tests that hypercall addresses are translated appropriatedly.
 *
 *  The virtual address 0x01000000 (16 MB) is mapped in a non-direct
 *  way (to 0x0100f000, 15 pages higher), then the stack pointer is
 *  shifted to the top of that page (0x01000FF0), then a hypercall
 *  is made to print a string.
 *
 *  You would naively think that would fail horribly unless hv_setpd()
 *  is working, but you would be wrong.  If hv_setpd() does absolutely
 *  nothing, then virtual 0x01000000 remains mapped to physical
 *  0x01000000, the stack works fine, and the test passes.  This is
 *  not wild speculation; is was observed S'19.
 *
 *  To combat this, we can't use a string in the kernel's rodata,
 *  which never "moves".  So before we install the new warped page
 *  tables we put a string at the bottom of the "physical" page,
 *  install the new warped page tables, swing the stack pointer
 *  to the warped virtual address, and print the string using the
 *  warped virtual address.  If the warped mapping wasn't established
 *  the string shouldn't print.
 *
 *  If the test works right, the host kernel should display a greeting
 *  message in the guest's console window, a success message should
 *  be printed on the Simics console (if running under Simics), and
 *  then the guest should exit.
 *
 *  @author rpearl
 *  @author de0u
 *
 *  PebPeb-only executable -- it doesn't work on hardware.
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

#include <assert.h>
#include <string.h>

#include <warp.h>

/* We're going to construct a page directory and page table
 * which is not direct-mapped.
 *
 * So, in C, we're going to construct a vm with kernel direct-mapped,
 * and also have the top most page table mapped. The topmost page will
 * be mapped... to a frame from the kernel. This will be our stack.
 *
 * In assembly, we'll switch to that stack, and then make a hypercall.
 */


/** @brief Kernel entrypoint.
 *
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t *mbinfo, int argc, char **argv, char **envp) {

    char msg[] = "Hello down there!";
    int msglen = sizeof (msg) - 1;

    lprintf("Hello from a brand new kernel!");

    if (!hv_isguest()) {
        lprintf("I am NOT supposed to be here!!!");
        panic("This payload is guest-only");
        return (-1);
    }

    /* Install the string in physical memory where the virtual
     * address should be mapped to.
     */
    strcpy((char *)WARP_PADDR, msg);

    /* Right now, &dmap contains all the information we need.
     * But, it assumes that the page directory is at USER_MEM_START,
     * so we'll move it up there.
     */
    uint32_t *pd = (uint32_t *)USER_MEM_START;
    /* One frame for PD, and 5 PTs. */
    memcpy(pd, &dmap, 6 * PAGE_SIZE);

    lprintf("Remapping...");
    hv_setpd(pd, 0);

    lprintf("After stack warping, guest console should display:\"%s\"", msg);

    lprintf("Warping...");
    warp((void *)(WARP_VADDR + PAGE_SIZE - sizeof (uint32_t)), (char *)WARP_VADDR, msglen);
    lprintf("***** SURVIVED, CHECK GUEST CONSOLE FOR MESSAGE *****");

    hv_exit(0xfaded);

    lprintf("hv_exit() returned");
    return -1;
}
