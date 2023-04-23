/** @file syscall_hvcall.c
 *
 *  @brief miscellaneous syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <stdio.h>
#include <string.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <hvcall.h>
#include <hvcall_int.h>
#include <malloc_internal.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <assert.h>
#include <console.h>
#include <loader.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>
#include <usermem.h>

#define HV_REFPD_OP HV_RESERVED_0
#define HV_UNREFPD_OP HV_RESERVED_1
#define HV_LOADPD_OP HV_RESERVED_2

static void hvcall_exit(stack_frame_t* f);
static void hvcall_iret(stack_frame_t* f);
static void hvcall_setidt(stack_frame_t* f);
static void hvcall_setpd(stack_frame_t* f);
static void hvcall_adjustpg(stack_frame_t* f);
static void hvcall_print(stack_frame_t* f);
static void hvcall_set_color(stack_frame_t* f);
static void hvcall_set_cursor(stack_frame_t* f);
static void hvcall_get_cursor(stack_frame_t* f);
static void hvcall_print_at(stack_frame_t* f);

static void hvcall_refpd(int ref);
static void hvcall_loadpd(stack_frame_t* f);

// 0: processed
int pv_handle_syscall(int index, stack_frame_t* f) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    if (pv == NULL) {
        return -1;
    }
    if (f->eip >= USER_MEM_START) {
        pv_idt_entry_t* idt = pv_classify_interrupt(pv, index);
        if (idt->eip == 0) {
            goto no_idt_handler;
        }
        if (idt->desc != VIDT_DPL_3) {
            idt = &pv->vidt.fault[SWEXN_CAUSE_PROTFAULT - PV_FAULT_START];
            if (idt->eip == 0) {
                goto no_idt_handler;
            }
        }
        pv_inject_interrupt(t, pv, f, 0, idt->eip);
        return 0;
    }
    pv_die("Syscall is not allowed for PV kernels");

no_idt_handler:
    pv_die("No interrupt handler installed");
    return 1;
}

void sys_hvcall_real(stack_frame_t* f) {
    pv_t* pv = get_current()->process->pv;
    if (pv == NULL || f->eip >= USER_MEM_START) {
        return;
    }
    switch (f->eax) {
        case HV_MAGIC_OP:
            f->eax = (reg_t)HV_MAGIC;
            break;
        case HV_EXIT_OP:
            hvcall_exit(f);
            break;
        case HV_IRET_OP:
            hvcall_iret(f);
            break;
        case HV_SETIDT_OP:
            hvcall_setidt(f);
            break;
        case HV_DISABLE_OP:
            pv_mask_interrupt(pv);
            break;
        case HV_ENABLE_OP:
            pv_unmask_interrupt(pv);
            break;
        case HV_SETPD_OP:
            hvcall_setpd(f);
            break;
        case HV_ADJUSTPG_OP:
            hvcall_adjustpg(f);
            break;
        case HV_PRINT_OP:
            hvcall_print(f);
            break;
        case HV_SET_COLOR_OP:
            hvcall_set_color(f);
            break;
        case HV_SET_CURSOR_OP:
            hvcall_set_cursor(f);
            break;
        case HV_GET_CURSOR_OP:
            hvcall_get_cursor(f);
            break;
        case HV_PRINT_AT_OP:
            hvcall_print_at(f);
            break;
        case HV_REFPD_OP:
            hvcall_refpd(1);
            break;
        case HV_UNREFPD_OP:
            hvcall_refpd(0);
            break;
        case HV_LOADPD_OP:
            hvcall_loadpd(f);
            break;
        default:
            pv_die("Bad hvcall number");
            break;
    }
}

static void hvcall_exit(stack_frame_t* f) {
    reg_t esp = f->esp;
    int status;
    if (copy_from_user(esp, sizeof(int), &status) != 0) {
        goto read_arg_fail;
    }
    get_current()->process->exit_value = status;
    kill_current();

read_arg_fail:
    pv_die("Bad argument address");
}

#define EFLAGS_PV_MASK                                                       \
    (EFL_CF | EFL_PF | EFL_AF | EFL_ZF | EFL_SF | EFL_TF | EFL_DF | EFL_OF | \
     EFL_RF)

static void hvcall_iret(stack_frame_t* f) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    reg_t esp = f->esp;
    reg_t regs[5];
    if (copy_from_user(esp, 5 * sizeof(reg_t), regs) != 0) {
        goto read_arg_fail;
    }
    f->eip = regs[0];
    reg_t eflags_user = (regs[1] & EFLAGS_PV_MASK);
    reg_t eflags_kernel = (regs[1] & (~EFLAGS_PV_MASK));
    if ((eflags_kernel & (~(EFL_IF | EFL_RESV1))) != EFL_IOPL_RING0) {
        goto bad_eflags;
    }
    if ((eflags_kernel & EFL_IF) != 0) {
        pv_unmask_interrupt(pv);
    } else {
        pv_mask_interrupt(pv);
    }
    f->eflags = (eflags_user | DEFAULT_EFLAGS);
    f->esp = regs[2];
    if (regs[3] != 0) {
        pv->vesp0 = regs[3];
        pv_switch_mode(t->process, 0);
    }
    f->eax = regs[4];
    return;

bad_eflags:
    pv_die("Bad eflags value in iret");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_setidt(stack_frame_t* f) {
    reg_t esp = f->esp;
    int index;
    va_t eip0;
    int is_dpl0;
    if (copy_from_user(esp, sizeof(int), &index) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(va_t), &eip0) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + 2 * sizeof(va_t), sizeof(int), &is_dpl0) != 0) {
        goto read_arg_fail;
    }
    if (index < 0 || index >= IDT_ENTS) {
        goto bad_idt_index;
    }
    pv_t* pv = get_current()->process->pv;
    pv_idt_entry_t* idt = pv_classify_interrupt(pv, index);
    idt->eip = eip0;
    idt->desc = ((idt->desc & (~VIDT_DPL_MASK)) |
                 (is_dpl0 != 0 ? VIDT_DPL_0 : VIDT_DPL_3));
    return;

bad_idt_index:
    pv_die("Bad IDT index");
read_arg_fail:
    pv_die("Bad argument address");
}

static pv_pd_t* translate_pv_pd(pv_t* pv, pa_t pd, int wp);

static void hvcall_setpd(stack_frame_t* f) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    reg_t esp = f->esp;
    pa_t pd;
    int wp;
    if (copy_from_user(esp, sizeof(pa_t), &pd) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(int), &wp) != 0) {
        goto read_arg_fail;
    }
    if ((pd & PAGE_OFFSET_MASK) != 0) {
        goto bad_cr3;
    }
    pv_pd_t* pv_pd = translate_pv_pd(pv, pd, wp);
    if (pv_pd == NULL) {
        goto alloc_pd_fail;
    }
    queue_insert_head(&pv->shadow_pds, &pv_pd->pv_link);
    pv_select_pd(t->process, pv_pd);
    return;

alloc_pd_fail:
    pv_die("Page table translation failed");
bad_cr3:
    pv_die("Bad page directory address");
read_arg_fail:
    pv_die("Bad argument address");
}

#define PDE_RESV_MASK 0xf00
#define PTE_RESV_MASK 0xe00

static pv_pd_t* translate_pv_pd(pv_t* pv, pa_t pd, int wp) {
    pa_t mem_limit = pv->n_pages * PAGE_SIZE;
    pa_t overflow_pa = machine_phys_frames() * PAGE_SIZE;

    char* temp_space = smalloc(4 * PAGE_SIZE);
    if (temp_space == NULL) {
        goto alloc_temp_fail;
    }
    page_directory_t* t_pd = (page_directory_t*)(temp_space);
    page_table_t* t_pt = (page_table_t*)(temp_space + PAGE_SIZE);
    page_directory_t* t_user_pd =
        (page_directory_t*)(temp_space + 2 * PAGE_SIZE);
    page_table_t* t_user_pt = (page_table_t*)(temp_space + 3 * PAGE_SIZE);

    pv_pd_t* pv_pd = smalloc(sizeof(pv_pd_t));
    if (pv_pd == NULL) {
        goto alloc_pv_pd_fail;
    }
    pa_t cr3 = alloc_user_pages(1);
    if (cr3 == BAD_PA) {
        goto alloc_cr3_fail;
    }
    int old_if = save_clear_if();
    memset((void*)map_phys_page(cr3, NULL), 0, PAGE_SIZE);
    restore_if(old_if);

    pa_t user_cr3 = alloc_user_pages(1);
    if (user_cr3 == BAD_PA) {
        free_user_pages(cr3, 1);
        goto alloc_user_cr3_fail;
    }
    old_if = save_clear_if();
    memset((void*)map_phys_page(user_cr3, NULL), 0, PAGE_SIZE);
    restore_if(old_if);

    memset(t_pd, 0, PAGE_SIZE);
    memset(t_user_pd, 0, PAGE_SIZE);
    int i;
    for (i = 0; i < USER_PD_START; i++) {
        (*t_pd)[i] = (*kernel_pd)[i];
        (*t_user_pd)[i] = (*kernel_pd)[i];
    }
    old_if = save_clear_if();
    if (pd >= mem_limit) {
        goto bad_pt;
    }
    pd += pv->mem_base;
    for (i = 0; i < NUM_PAGE_ENTRY - USER_PD_START; i++) {
        page_directory_t* old_pd = (page_directory_t*)map_phys_page(pd, NULL);
        pde_t old_pde = (*old_pd)[i];
        if ((old_pde & (PTE_PRESENT << PTE_P_SHIFT)) == 0) {
            continue;
        }
        pa_t pt_pa = get_page_table(old_pde);
        if (pt_pa >= mem_limit) {
            goto bad_pt;
        }
        pt_pa += pv->mem_base;
        page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
        int j;
        for (j = 0; j < NUM_PAGE_ENTRY; j++) {
            pte_t old_pte = (*pt)[j];
            if ((old_pte & (PTE_PRESENT << PTE_P_SHIFT)) == 0) {
                (*t_pt)[j] = BAD_PTE;
                (*t_user_pt)[j] = BAD_PTE;
                continue;
            }
            pa_t pa = get_page_base(old_pte);
            if (pa > mem_limit) {
                pa = overflow_pa;
            } else {
                pa = pv->mem_base + pa;
            }
            int old_us = (old_pte & (1 << PTE_US_SHIFT));
            int old_rw = (old_pte & (1 << PTE_RW_SHIFT));
            int new_mask, new_user_mask;
            if (wp == 0) {
                new_mask = ((1 << PTE_P_SHIFT) | (PTE_RW << PTE_RW_SHIFT) |
                            (PTE_USER << PTE_US_SHIFT));
            } else {
                new_mask =
                    ((1 << PTE_P_SHIFT) | old_rw | (PTE_USER << PTE_US_SHIFT));
            }
            if (old_us != 0) {
                new_user_mask =
                    ((1 << PTE_P_SHIFT) | old_rw | (PTE_USER << PTE_US_SHIFT));
            } else {
                new_user_mask = 0;
            }
            (*t_pt)[j] =
                ((pa & PAGE_BASE_MASK) | (old_pte & PTE_RESV_MASK) | new_mask);
            (*t_user_pt)[j] = (new_user_mask == 0 ? BAD_PTE
                                                  : ((pa & PAGE_BASE_MASK) |
                                                     (old_pte & PTE_RESV_MASK) |
                                                     new_user_mask));
        }
        pa_t new_pt = alloc_user_pages(1);
        if (new_pt == BAD_PA) {
            goto alloc_pt_fail;
        }
        pa_t new_user_pt = alloc_user_pages(1);
        if (new_user_pt == BAD_PA) {
            free_user_pages(new_pt, 1);
            goto alloc_pt_fail;
        }
        memcpy((void*)map_phys_page(new_pt, NULL), t_pt, PAGE_SIZE);
        memcpy((void*)map_phys_page(new_user_pt, NULL), t_user_pt, PAGE_SIZE);
        pde_t new_pde = ((old_pde & PDE_RESV_MASK) |
                         (PTE_USER << PTE_US_SHIFT) | (1 << PTE_P_SHIFT));
        if (wp == 0) {
            new_pde |= (PTE_RW << PTE_RW_SHIFT);
        } else {
            new_pde |= (old_pde & (1 << PTE_RW_SHIFT));
        }
        (*t_pd)[USER_PD_START + i] = (new_pt | new_pde);
        (*t_user_pd)[USER_PD_START + i] = (new_user_pt | new_pde);
    }
    memcpy((void*)map_phys_page(cr3, NULL), t_pd, PAGE_SIZE);
    memcpy((void*)map_phys_page(user_cr3, NULL), t_user_pd, PAGE_SIZE);
    restore_if(old_if);
    sfree(temp_space, 4 * PAGE_SIZE);
    pv_pd->guest_pd = pd;
    pv_pd->wp = wp;
    pv_pd->cr3 = cr3;
    pv_pd->user_cr3 = user_cr3;
    pv_pd->refcount = 0;
    return pv_pd;

bad_pt:
alloc_pt_fail:
    memcpy((void*)map_phys_page(cr3, NULL), t_pd, PAGE_SIZE);
    memcpy((void*)map_phys_page(user_cr3, NULL), t_user_pd, PAGE_SIZE);
    restore_if(old_if);
    destroy_pd(cr3);
    destroy_pd(user_cr3);
alloc_user_cr3_fail:
alloc_cr3_fail:
    sfree(pv_pd, sizeof(pv_pd_t));
alloc_pv_pd_fail:
    sfree(temp_space, 4 * PAGE_SIZE);
alloc_temp_fail:
    return NULL;
}

static void hvcall_adjustpg(stack_frame_t* f) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    pv_pd_t* pv_pd = pv->active_shadow_pd;
    reg_t esp = f->esp;
    va_t addr;
    if (copy_from_user(esp, sizeof(va_t), &addr) != 0) {
        goto read_arg_fail;
    }
    if ((addr & PAGE_OFFSET_MASK) != 0 || addr >= PV_VM_LIMIT) {
        goto bad_addr;
    }
    pa_t mem_limit = pv->n_pages * PAGE_SIZE;

    pa_t pt_pa, user_pt_pa;
    int old_if = save_clear_if();
    page_directory_t* old_pd =
        (page_directory_t*)map_phys_page(pv_pd->guest_pd, NULL);
    pde_t old_pde = (*old_pd)[get_pd_index(addr)];
    if ((old_pde & (1 << PTE_P_SHIFT)) == 0) {
        page_directory_t* pd =
            (page_directory_t*)map_phys_page(pv_pd->cr3, NULL);
        pde_t* pde = &(*pd)[get_pd_index(addr) + USER_PD_START];
        if (*pde != BAD_PDE) {
            pt_pa = get_page_table(*pde);
            *pde = BAD_PDE;
            free_user_pages(pt_pa, 1);
        }
        page_directory_t* user_pd =
            (page_directory_t*)map_phys_page(pv_pd->user_cr3, NULL);
        pde_t* user_pde = &(*user_pd)[get_pd_index(addr) + USER_PD_START];
        if (*user_pde != BAD_PDE) {
            user_pt_pa = get_page_table(*user_pde);
            *user_pde = BAD_PDE;
            free_user_pages(user_pt_pa, 1);
        }
    } else {
        pde_t new_pde = ((old_pde & PDE_RESV_MASK) |
                         (PTE_USER << PTE_US_SHIFT) | (1 << PTE_P_SHIFT));
        if (pv_pd->wp == 0) {
            new_pde |= (PTE_RW << PTE_RW_SHIFT);
        } else {
            new_pde |= (old_pde & (1 << PTE_RW_SHIFT));
        }
        page_directory_t* pd =
            (page_directory_t*)map_phys_page(pv_pd->cr3, NULL);
        pde_t* pde = &(*pd)[get_pd_index(addr) + USER_PD_START];
        if (*pde != BAD_PDE) {
            pt_pa = get_page_table(*pde);
            *pde = (pt_pa | new_pde);
        } else {
            pt_pa = alloc_user_pages(1);
            if (pt_pa == BAD_PA) {
                goto alloc_pt_fail;
            }
            memset((void*)map_phys_page(pt_pa, NULL), 0, PAGE_SIZE);
            map_phys_page(pv_pd->cr3, NULL);
            *pde = (pt_pa | new_pde);
        }
        page_directory_t* user_pd =
            (page_directory_t*)map_phys_page(pv_pd->user_cr3, NULL);
        pde_t* user_pde = &(*user_pd)[get_pd_index(addr) + USER_PD_START];
        if (*user_pde != BAD_PDE) {
            user_pt_pa = get_page_table(*user_pde);
            *user_pde = (user_pt_pa | new_pde);
        } else {
            user_pt_pa = alloc_user_pages(1);
            if (user_pt_pa == BAD_PA) {
                goto alloc_pt_fail;
            }
            memset((void*)map_phys_page(user_pt_pa, NULL), 0, PAGE_SIZE);
            map_phys_page(pv_pd->user_cr3, NULL);
            *user_pde = (user_pt_pa | new_pde);
        }
        pa_t old_pt_pa = get_page_table(old_pde);
        if (old_pt_pa >= mem_limit) {
            goto bad_pt;
        }
        old_pt_pa += pv->mem_base;
        page_table_t* old_pt = (page_table_t*)map_phys_page(old_pt_pa, NULL);
        pte_t old_pte = (*old_pt)[get_pt_index(addr)];
        pte_t pte, user_pte;
        if ((old_pte & (PTE_PRESENT << PTE_P_SHIFT)) == 0) {
            user_pte = pte = BAD_PTE;
        } else {
            pa_t pa = get_page_base(old_pte);
            if (pa > mem_limit) {
                pa = machine_phys_frames() * PAGE_SIZE;
            } else {
                pa = pv->mem_base + pa;
            }
            int old_us = (old_pte & (1 << PTE_US_SHIFT));
            int old_rw = (old_pte & (1 << PTE_RW_SHIFT));
            int new_mask, new_user_mask;
            if (pv_pd->wp == 0) {
                new_mask = ((1 << PTE_P_SHIFT) | (PTE_RW << PTE_RW_SHIFT) |
                            (PTE_USER << PTE_US_SHIFT));
            } else {
                new_mask =
                    ((1 << PTE_P_SHIFT) | old_rw | (PTE_USER << PTE_US_SHIFT));
            }
            if (old_us != 0) {
                new_user_mask =
                    ((1 << PTE_P_SHIFT) | old_rw | (PTE_USER << PTE_US_SHIFT));
            } else {
                new_user_mask = 0;
            }
            pte =
                ((pa & PAGE_BASE_MASK) | (old_pte & PTE_RESV_MASK) | new_mask);
            user_pte = (new_user_mask == 0
                            ? BAD_PTE
                            : ((pa & PAGE_BASE_MASK) |
                               (old_pte & PTE_RESV_MASK) | new_user_mask));
        }
        page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
        (*pt)[get_pt_index(addr)] = pte;
        page_table_t* user_pt = (page_table_t*)map_phys_page(user_pt_pa, NULL);
        (*user_pt)[get_pt_index(addr)] = user_pte;
    }
    restore_if(old_if);
    invlpg(addr + USER_MEM_START);
    return;

bad_pt:
    restore_if(old_if);
    pv_die("Bad page table address");
alloc_pt_fail:
    restore_if(old_if);
    pv_die("Failed to alloc page table");
bad_addr:
    pv_die("Unaligned virtual address");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_print(stack_frame_t* f) {
    reg_t esp = f->esp;
    int len;
    va_t base;
    if (copy_from_user(esp, sizeof(int), &len) != 0) {
        goto read_arg_fail;
    }
    if (len < 0) {
        goto bad_length;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(va_t), &base) != 0) {
        goto read_arg_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    int result = print_buf_from_user(pts, base, len);
    mutex_unlock(&pts->lock);
    if (result != 0) {
        goto print_fail;
    }
    return;

print_fail:
    pv_die("Error when printing buffer");
bad_length:
    pv_die("Bad buffer length");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_set_color(stack_frame_t* f) {
    reg_t esp = f->esp;
    int color;
    if (copy_from_user(esp, sizeof(int), &color) != 0) {
        goto read_arg_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    int result = pts_set_term_color(pts, color);
    mutex_unlock(&pts->lock);
    if (result != 0) {
        goto set_fail;
    }
    return;

set_fail:
    pv_die("Error when setting color");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_set_cursor(stack_frame_t* f) {
    reg_t esp = f->esp;
    int row, col;
    if (copy_from_user(esp, sizeof(int), &row) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(int), &col) != 0) {
        goto read_arg_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    int result = pts_set_cursor(pts, row, col);
    mutex_unlock(&pts->lock);
    if (result != 0) {
        goto set_fail;
    }
    return;

set_fail:
    pv_die("Error when setting cursor position");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_get_cursor(stack_frame_t* f) {
    reg_t esp = f->esp;
    va_t prow, pcol;
    if (copy_from_user(esp, sizeof(va_t), &prow) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(va_t), &pcol) != 0) {
        goto read_arg_fail;
    }
    int row, col;
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    pts_get_cursor(pts, &row, &col);
    mutex_unlock(&pts->lock);
    if (copy_to_user(prow, sizeof(int), &row) != 0) {
        goto bad_arg;
    }
    if (copy_to_user(pcol, sizeof(int), &col) != 0) {
        goto bad_arg;
    }
    return;

bad_arg:
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_print_at(stack_frame_t* f) {
    reg_t esp = f->esp;
    int len;
    va_t base;
    int row, col, old_row, old_col;
    int color, old_color;
    if (copy_from_user(esp, sizeof(int), &len) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + sizeof(va_t), sizeof(va_t), &base) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + 2 * sizeof(va_t), sizeof(int), &row) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + 3 * sizeof(va_t), sizeof(int), &col) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user(esp + 4 * sizeof(va_t), sizeof(int), &color) != 0) {
        goto read_arg_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    pts_get_cursor(pts, &old_row, &old_col);
    if (pts_set_cursor(pts, row, col) != 0) {
        goto bad_pos;
    }
    pts_get_term_color(pts, &old_color);
    if (pts_set_term_color(pts, color) != 0) {
        goto bad_color;
    }
    if (print_buf_from_user(pts, base, len) != 0) {
        goto bad_print;
    }
    pts_set_term_color(pts, old_color);
    pts_set_cursor(pts, old_row, old_col);
    mutex_unlock(&pts->lock);
    return;

bad_print:
    pts_set_term_color(pts, old_color);
bad_color:
    pts_set_cursor(pts, old_row, old_col);
bad_pos:
    mutex_unlock(&pts->lock);
    pv_die("Bad argument");
read_arg_fail:
    pv_die("Bad argument address");
}

static void hvcall_refpd(int ref) {
    pv_pd_t* pv_pd = get_current()->process->pv->active_shadow_pd;
    if (ref == 1) {
        pv_pd->refcount++;
    } else {
        pv_pd->refcount--;
        if (pv_pd->refcount <= 0) {
            pv_die("Page table destroyed by kerenl");
        }
    }
}

static void hvcall_loadpd(stack_frame_t* f) {
    reg_t esp = f->esp;
    pa_t pd;
    if (copy_from_user(esp, sizeof(pa_t), &pd) != 0) {
        goto read_arg_fail;
    }
    process_t* p = get_current()->process;
    pv_t* pv = p->pv;
    queue_t* node = pv->shadow_pds;
    queue_t* end = node;
    do {
        pv_pd_t* pv_pd = queue_data(node, pv_pd_t, pv_link);
        if (pv_pd->guest_pd == pd + USER_MEM_START) {
            pv_select_pd(p, pv_pd);
            return;
        }
        node = node->next;
        sfree(pv_pd, sizeof(pv_pd_t));
    } while (node != end);
    pv_die("Loading a nonexist page table");

read_arg_fail:
    pv_die("Bad argument address");
}
