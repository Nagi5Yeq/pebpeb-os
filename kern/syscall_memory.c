/** @file syscall_memory.c
 *
 *  @brief memory syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <string.h>
#include <common_kern.h>
#include <elf/elf_410.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <usermem.h>

/**
 * @brief new_pages() syscall handler
 * @param f saved regs
 */
void sys_new_pages_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    va_t base;
    if (copy_from_user((va_t)esi, sizeof(va_t), &base) != 0) {
        goto read_fail;
    }
    if ((base & PAGE_OFFSET_MASK) != 0) {
        goto read_fail;
    }
    int len;
    if (copy_from_user((va_t)esi + sizeof(va_t), sizeof(int), &len) != 0) {
        goto read_fail;
    }
    if ((len & PAGE_OFFSET_MASK) != 0) {
        goto read_fail;
    }

    process_t* p = get_current()->process;
    mutex_lock(&p->mm_lock);
    int n_pages = len / PAGE_SIZE;
    pa_t paddr = alloc_user_pages(n_pages);
    if (paddr == 0) {
        goto alloc_segment_fail;
    }
    if (add_region(p, base, n_pages, paddr, 1) != 0) {
        goto add_region_fail;
    }

    int i = 0;
    int offset = 0;
    /* file page table one entry per loop */
    pa_t pt_pa = find_or_create_pt(p, base);
    if (pt_pa == BAD_PA) {
        goto add_pt_fail;
    }
    do {
        offset = i * PAGE_SIZE;
        /* step to next page table */
        int pt_index = get_pt_index(base + offset);
        if (pt_index == 0) {
            pt_pa = find_or_create_pt(p, base + offset);
            if (pt_pa == BAD_PA) {
                goto add_pt_fail;
            }
        }

        int old_if = save_clear_if();
        page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
        (*pt)[pt_index] = make_pte(paddr + offset, 0, PTE_USER, PTE_RW, 0);
        restore_if(old_if);
        invlpg(base + offset);
    } while (++i < n_pages);
    mutex_unlock(&p->mm_lock);
    f->eax = 0;
    return;

add_pt_fail:
    /* page tables are reachable from page directory so no need to free them */
    vector_pop(&p->regions);
add_region_fail:
    free_user_pages(paddr, n_pages);
alloc_segment_fail:
    mutex_unlock(&p->mm_lock);
read_fail:
    f->eax = (reg_t)-1;
}

/**
 * @brief remove_pages() syscall handler
 * @param f saved regs
 */
void sys_remove_pages_real(stack_frame_t* f) {
    process_t* p = get_current()->process;
    mutex_lock(&p->mm_lock);
    va_t base = (va_t)f->esi;
    int i, j, n = vector_size(&p->regions);
    for (i = 0; i < n; i++) {
        region_t* r = (region_t*)vector_at(&p->regions, i);
        if (base == r->addr) {
            int n_pages = r->size / PAGE_SIZE;
            j = 0;
            int offset = 0;
            pa_t pt_pa = find_or_create_pt(p, base);
            do {
                offset = j * PAGE_SIZE;
                int pt_index = get_pt_index(base + offset);
                if (pt_index == 0) {
                    pt_pa = find_or_create_pt(p, base + offset);
                }

                int old_if = save_clear_if();
                page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
                (*pt)[pt_index] = BAD_PDE;
                restore_if(old_if);
                invlpg(base + offset);
            } while (++j < n_pages);

            free_user_pages(r->paddr, r->size / PAGE_SIZE);
            vector_remove(&p->regions, i);
            mutex_unlock(&p->mm_lock);
            f->eax = 0;
            return;
        }
    }
    /* no such region */
    mutex_unlock(&p->mm_lock);
    f->eax = (reg_t)-1;
}
