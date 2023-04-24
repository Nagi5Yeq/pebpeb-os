/** @file paging.h
 *
 *  @brief paging management functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _PAGING_H_
#define _PAGING_H_

#include <x86/page.h>

/* SOMEONE SUGGESTED THAT I SHOULD USE UINTPTR_T FOR PORTABILITY BUT I DON'T SEE
 * ANY USE OF IT IN ANY PROVIDED CODE AND THIS COURSE HAVE ALREADY SAY WE ARE
 * TARGETING X86 ARCHTECTURE SO I JUST USE A 'NON-PORTABLE' INT TYPE */

/** an entry in page table */
typedef unsigned long pte_t;
/** an entry in page directory */
typedef unsigned long pde_t;

/** virtual address */
typedef unsigned long va_t;

/** physical address */
typedef unsigned long pa_t;

/** size of memory */
typedef unsigned long va_size_t;

/** a register */
typedef unsigned int reg_t;

#ifndef PAGE_OFFSET_MASK
/** mask of page offset bits */
#define PAGE_OFFSET_MASK (PAGE_SIZE - 1)
#endif

#ifndef PAGE_BASE_MASK
/** mask of page base bits */
#define PAGE_BASE_MASK (~PAGE_OFFSET_MASK)
#endif

/** index of entry in page directory where user memory starts */
#define USER_PD_START (USER_MEM_START / PAGE_SIZE / NUM_PAGE_ENTRY)

/** means a page is present */
#define PTE_PRESENT 1
/** means a page is writeable */
#define PTE_RW 1
/** means a page is not writeable */
#define PTE_RO 0
/** means a page can be accessed in ring 3 */
#define PTE_USER 1
/** means a page can only be accessed in ring 0 */
#define PTE_SUPERVISOR 0
#define PTE_PCD 1
#define PTE_PWT 1
/** means a page's TLB entry should not be flushed on switching cr3 */
#define PTE_G 1

/** P bit's position */
#define PTE_P_SHIFT 0
/** RW/RO bit's position */
#define PTE_RW_SHIFT 1
/** USER/SUPERVISOR bit's position */
#define PTE_US_SHIFT 2
/** PWT bit's position */
#define PTE_PWT_SHIFT 3
/** PCD bit's position */
#define PTE_PCD_SHIFT 4
/** G bit's position */
#define PTE_G_SHIFT 8

/** an invalid page directory entry */
#define BAD_PDE ((pde_t)0)
/** an invalid page table entry */
#define BAD_PTE ((pte_t)0)

/**
 * @brief create a page table entry
 * @param base address of the page
 * @param g g bit
 * @param us user/supervisor bit
 * @param rw rw/ro bit
 * @param p present bit
 * @return the page table entry
 */
static inline pte_t make_pte(pa_t base, int g, int us, int rw, int p) {
    return ((base & PAGE_BASE_MASK) | (g << PTE_G_SHIFT) |
            (us << PTE_US_SHIFT) | (rw << PTE_RW_SHIFT) | (p << PTE_P_SHIFT));
}

/**
 * @brief create a page directory entry
 * @param base address of the page
 * @param us user/supervisor bit
 * @param rw rw/ro bit
 * @param p present bit
 * @return the page directory entry
 */
static inline pde_t make_pde(pa_t base, int us, int rw, int p) {
    return ((base & PAGE_BASE_MASK) | (us << PTE_US_SHIFT) |
            (rw << PTE_RW_SHIFT) | (p << PTE_P_SHIFT));
}

#ifndef NUM_PAGE_ENTRY
/** number of entries in a page table */
#define NUM_PAGE_ENTRY (PAGE_SIZE / sizeof(pte_t))
#endif

/** size of memory a page table can manage */
#define PT_SIZE (PAGE_SIZE * NUM_PAGE_ENTRY)

/** page directory type */
typedef pde_t page_directory_t[PAGE_SIZE / sizeof(pde_t)];
/** page table type */
typedef pte_t page_table_t[PAGE_SIZE / sizeof(pde_t)];

/** get page table address from a pde */
static inline pa_t get_page_table(pde_t pde) {
    return (pa_t)(pde & PAGE_BASE_MASK);
}

/** get page base address from a pte */
static inline pa_t get_page_base(pte_t pte) {
    return (pa_t)(pte & PAGE_BASE_MASK);
}

/** get index in page directory of an address */
static inline int get_pd_index(va_t va) {
    return va / PAGE_SIZE / NUM_PAGE_ENTRY;
}

/** get index in page table of an address */
static inline int get_pt_index(va_t va) {
    return (va / PAGE_SIZE) % NUM_PAGE_ENTRY;
}

/** kernel's page directory which directly maps all kernel memory */
extern page_directory_t* kernel_pd;
/** area for mapping physical pages */
extern va_t mapped_phys_pages;
/** location of the page table entry of the mapping area */
extern pte_t* mapped_phys_page_ptes;

/**
 * @brief initialize and enable paging
 */
void paging_init();

/**
 * @brief enable paging
 */
void paging_enable();

#endif
