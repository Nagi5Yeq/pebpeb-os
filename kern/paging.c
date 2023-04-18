/** @file paging.c
 *
 *  @brief paging management functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <common_kern.h>
#include <malloc_internal.h>
#include <stdio.h>
#include <string.h>

#include <apic.h>
#include <mptable.h>
#include <smp.h>
#include <x86/asm.h>
#include <x86/cr.h>

#include <asm_instr.h>
#include <paging.h>
#include <sched.h>

/** number of page tables for kernel memory */
#define NUM_KERNEL_PT (USER_MEM_START / PAGE_SIZE / NUM_PAGE_ENTRY)

page_directory_t* kernel_pd;
/** page tables for kernel memory */
static page_table_t* kernel_pt;

va_t mapped_phys_pages;
pte_t* mapped_phys_page_ptes;

void paging_init() {
    kernel_pd = (page_directory_t*)_smemalign(PAGE_SIZE, PAGE_SIZE);
    kernel_pt = (page_table_t*)_smemalign(PAGE_SIZE, PAGE_SIZE * NUM_KERNEL_PT);

    int num_cpus = smp_num_cpus();
    mapped_phys_pages = (va_t)_smemalign(PAGE_SIZE, num_cpus * PAGE_SIZE);
    mapped_phys_page_ptes = (pte_t*)kernel_pt + (mapped_phys_pages / PAGE_SIZE);

    if (kernel_pd == NULL || kernel_pt == NULL || mapped_phys_pages == 0 ||
        mapped_phys_page_ptes == NULL) {
        panic("no space for kernel page table");
    }

    int i, j;
    pa_t base = 0;
    memset(kernel_pd, 0, sizeof(page_directory_t));
    for (i = 0; i < (USER_MEM_START / PT_SIZE); i++) {
        (*kernel_pd)[i] =
            make_pde((pa_t)&kernel_pt[i], PTE_SUPERVISOR, PTE_RW, PTE_PRESENT);
        for (j = 0; j < NUM_PAGE_ENTRY; j++) {
            kernel_pt[i][j] =
                make_pte(base, PTE_G, PTE_SUPERVISOR, PTE_RW, PTE_PRESENT);
            base += PAGE_SIZE;
        }
    }

    pa_t lapic_pa = (pa_t)smp_lapic_base();
    pte_t* lapic_pte = (pte_t*)kernel_pt + (LAPIC_VIRT_BASE / PAGE_SIZE);
    *lapic_pte =
        (make_pte(lapic_pa, PTE_G, PTE_SUPERVISOR, PTE_RW, PTE_PRESENT) |
         (PTE_PCD << PTE_PCD_SHIFT));
    invlpg(LAPIC_VIRT_BASE);

    set_cr4(get_cr4() | CR4_PSE | CR4_PGE);
    set_cr3((uint32_t)kernel_pd);
    set_cr0(get_cr0() | CR0_PE | CR0_PG);
}

void paging_enable() {
    set_cr4(get_cr4() | CR4_PSE | CR4_PGE);
    set_cr3((uint32_t)kernel_pd);
    set_cr0(get_cr0() | CR0_PE | CR0_PG);
}
