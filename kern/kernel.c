/** @file kernel.c
 *
 *  @brief kernel entry.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <common_kern.h>

/* libc includes. */
#include <simics.h> /* lprintf() */
#include <stdio.h>
#include <string.h>

/* multiboot header file */
#include <mptable.h>
#include <multiboot.h> /* boot_info */
#include <smp.h>

/* x86 specific includes */
#include <x86/asm.h> /* enable_interrupts() */
#include <x86/cr.h>

#include <assert.h>
#include <common.h>
#include <interrupt.h>
#include <mm.h>
#include <paging.h>
#include <pts.h>
#include <pv.h>
#include <sched.h>
#include <timer.h>
#include <toad.h>

void print_toad();

/**
 * @brief entry for a cpu core
 * @param cpuid cpuid
 */
static void kernel_smp_entry(int cpuid);

/**
 * @brief main function for a cpu core
 */
static void kernel_smp_main();

/** @brief Kernel entrypoint.
 *
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t* mbinfo, int argc, char** argv, char** envp) {
    int smp_good = (smp_init(mbinfo) == 0);
    paging_init();
    setup_pts();
    percpu_t percpu;
    setup_percpu(&percpu);
    thread_t kthread;
    process_t kprocess;
    setup_kth(&kthread, &kprocess);
    set_mapped_phys_page(mapped_phys_pages);
    set_mapped_phys_page_pte(mapped_phys_page_ptes);

    idt_init();
    mm_init();
    pv_init();
    timer_init();

    print_toad();

    const char* init_args[] = {INIT_NAME};
    thread_t* init = create_process(INIT_PID, INIT_NAME, 1, init_args);
    init_process = init->process;
    add_thread(init);
    insert_ready_tail(init);

    if (smp_good && smp_num_cpus() > 1) {
        set_cr3((pa_t)&kernel_pd);
        smp_boot(kernel_smp_entry);
    }

    setup_lapic_timer();
    kernel_smp_main();
    return -1;
}

static void kernel_smp_entry(int cpuid) {
    paging_enable();
    percpu_t percpu;
    setup_percpu(&percpu);
    thread_t kthread;
    process_t kprocess;
    setup_kth(&kthread, &kprocess);
    set_mapped_phys_page(mapped_phys_pages + cpuid * PAGE_SIZE);
    set_mapped_phys_page_pte(mapped_phys_page_ptes + cpuid);
    setup_lapic_timer();
    kernel_smp_main();
}

static void kernel_smp_main() {
    const char* idle_args[] = {IDLE_NAME};
    thread_t* idle = create_process(IDLE_PID, IDLE_NAME, 1, idle_args);
    set_idle(idle);

    while (1) {
        int old_if = spl_lock(&ready_lock);
        thread_t* t = select_next();
        yield_to_spl_unlock(t, &ready_lock, old_if);
        /* when a thread borrow kthread's stack and free itself, it will return
         * here and we continue yielding
         */
    }
    panic("kthread should not return");
}
