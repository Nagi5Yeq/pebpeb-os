/** @file mm.h
 *
 *  @brief memory management functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _MM_H_
#define _MM_H_

#include <syscall_int.h>

#include <paging.h>
#include <sync.h>

/** a invalid physical address when managing user memory */
#define BAD_PA 0

/** lock for kernel memory allocation */
extern mutex_t malloc_lock;

/** lock for user memory management */
extern mutex_t mm_lock;

/**
 * @brief initialize user memory system
 */
void mm_init();

/**
 * @brief allocate physical pages
 * @param num_pages number of pages to allocate
 * @return physical address of pages, or BAD_PA on failure
 */
pa_t alloc_user_pages(int num_pages);

/**
 * @brief free physical pages
 * @param pa physcial address of pages
 * @param num_pages number of pages to allocate
 */
void free_user_pages(pa_t pa, int num_pages);

/**
 * @brief map a physical page to kernel memory, the mapping area will be reused
 * after subsequent map_phys_page() calls
 * @param pa physcial address of the page
 * @param old_pa output argument to save old mapped page, can be NULL if no need
 * to save
 * @return virtual address of mapped page
 */
va_t map_phys_page(pa_t pa, pa_t* old_pa);

#endif
