#include <simics.h>
#include <stdio.h>
#include <string.h>

#include <hvcall.h>
#include <x86/asm.h>
#include <x86/cr.h>

#include <asm_instr.h>
#include <loader.h>
#include <mm.h>
#include <paging.h>
#include <pv.h>
#include <sched.h>
#include <usermem.h>

static int create_boot_pd(process_t* p, pa_t bootmem, int n_pages);

void pv_init() {
    uint64_t* gdt = (uint64_t*)gdt_base();
    /* copy cs's flags so we do not need to create one */
    uint64_t cs_flags = (gdt[SEGSEL_USER_CS_IDX] & GDT_FLAG_MASK);
    /* copy ds's flags so we do not need to create one */
    uint64_t ds_flags = (gdt[SEGSEL_USER_DS_IDX] & GDT_FLAG_MASK);
    uint64_t fs_flags = (ds_flags & (~GDT_G_BIT));
    va_t base = USER_MEM_START;
    va_size_t limit = (PV_VM_LIMIT / PAGE_SIZE) - 1;
    gdt[SEGSEL_PV_CS_IDX] = create_segsel(base, limit, cs_flags);
    gdt[SEGSEL_PV_DS_IDX] = create_segsel(base, limit, ds_flags);
    gdt[SEGSEL_PV_FS_IDX] = create_segsel(base, limit, fs_flags);
}

static int create_boot_pd(process_t* p, pa_t bootmem, int n_pages) {
    pv_pd_t* pv_pd = smalloc(sizeof(pv_pd_t));
    if (pv_pd == NULL) {
        goto alloc_pv_pd_fail;
    }
    pv_pd->refcount = 1;
    pv_pd->cr3 = p->cr3;
    pv_pd->user_cr3 = p->cr3;
    queue_insert_head(&p->pv->shadow_pds, &pv_pd->pv_link);
    p->pv->active_shadow_pd = pv_pd;

    int i = 0;
    int offset = 0;
    va_t m_start = USER_MEM_START;
    pa_t pt_pa = find_or_create_pt(p, m_start);
    if (pt_pa == BAD_PA) {
        goto add_pt_fail;
    }
    do {
        offset = i * PAGE_SIZE;
        int pt_index = get_pt_index(m_start + offset);
        if (pt_index == 0) {
            /* step to next page table */
            pt_pa = find_or_create_pt(p, m_start + offset);
            if (pt_pa == BAD_PA) {
                goto add_pt_fail;
            }
        }
        int old_if = save_clear_if();
        page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
        (*pt)[pt_index] =
            make_pte(bootmem + offset, 0, PTE_USER, PTE_RW, PTE_PRESENT);
        restore_if(old_if);
        invlpg(m_start + offset);
    } while (++i < n_pages);
    memset((char*)USER_MEM_START, 0, n_pages * PAGE_SIZE);
    return 0;

add_pt_fail:
alloc_pv_pd_fail:
    return -1;
}

thread_t* create_pv_process(thread_t* t,
                            simple_elf_t* elf,
                            char* exe,
                            va_size_t mem_size) {
    process_t* p = t->process;
    pv_t* pv = (pv_t*)smalloc(sizeof(pv_t));
    if (pv == NULL) {
        goto alloc_pv_fail;
    }
    pv->shadow_pds = NULL;
    pv->vif = 0;
    memset(&pv->vidt, 0, sizeof(pv_idt_t));
    p->pv = pv;

    int n_bootmem_pages = mem_size / PAGE_SIZE;
    pa_t bootmem = alloc_user_pages(n_bootmem_pages);
    if (bootmem == BAD_PA) {
        goto alloc_bootmem_fail;
    }
    pv->n_pages = n_bootmem_pages;
    pv->mem_base = bootmem;
    if (add_region(p, USER_MEM_START, n_bootmem_pages, bootmem, 1) != 0) {
        goto alloc_region_fail;
    }

    /* temporarily use new process's cr3 to load elf */
    pa_t old_cr3 = get_current()->process->cr3;
    get_current()->process->cr3 = p->cr3;
    set_cr3(p->cr3);

    if (create_boot_pd(p, bootmem, n_bootmem_pages) != 0) {
        goto create_boot_pd_fail;
    }

    file_t* f = find_file(exe);

    if (elf->e_txtlen != 0) {
        if (elf->e_txtstart >= mem_size) {
            goto load_elf_fail;
        }
        read_file(f, elf->e_txtoff, elf->e_txtlen,
                  (char*)(USER_MEM_START + elf->e_txtstart));
    }
    if (elf->e_rodatlen != 0) {
        if (elf->e_rodatstart >= mem_size) {
            goto load_elf_fail;
        }
        read_file(f, elf->e_rodatoff, elf->e_rodatlen,
                  (char*)(USER_MEM_START + elf->e_rodatstart));
    }
    if (elf->e_datlen != 0) {
        if (elf->e_datstart >= mem_size) {
            goto load_elf_fail;
        }
        read_file(f, elf->e_datoff, elf->e_datlen,
                  (char*)(USER_MEM_START + elf->e_datstart));
    }

    get_current()->process->cr3 = old_cr3;
    set_cr3(old_cr3);

    t->kernel_esp -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)t->kernel_esp;
    frame->eip = (reg_t)elf->e_entry;
    frame->cs = SEGSEL_PV_CS;
    frame->eflags = DEFAULT_EFLAGS;
    frame->esp = 0;
    frame->ss = SEGSEL_PV_DS;
    frame->eax = GUEST_LAUNCH_EAX;
    frame->ecx = PV_VM_LIMIT - 1;
    frame->edx = 0;
    frame->ebx = n_bootmem_pages - 1;
    frame->ebp = 0;
    frame->esi = 0;
    frame->edi = 0;
    frame->ds = SEGSEL_PV_DS;
    frame->es = SEGSEL_PV_DS;
    frame->fs = SEGSEL_PV_FS;
    frame->gs = SEGSEL_PV_DS;

    t->kernel_esp -= sizeof(yield_frame_t);
    yield_frame_t* yf = (yield_frame_t*)t->kernel_esp;
    yf->eflags = DEFAULT_EFLAGS;
    yf->raddr = (reg_t)return_to_user;
    return t;

load_elf_fail:
create_boot_pd_fail:
    get_current()->process->cr3 = old_cr3;
    set_cr3(old_cr3);
    vector_pop(&p->regions);
alloc_region_fail:
    free_user_pages(bootmem, n_bootmem_pages);
alloc_bootmem_fail:
alloc_pv_fail:
    destroy_thread(t);
    return NULL;
}

void destroy_pv(pv_t* pv) {
    if (pv->shadow_pds != NULL) {
        queue_t* node = pv->shadow_pds;
        queue_t* end = node;
        do {
            pv_pd_t* pv_pd = queue_data(node, pv_pd_t, pv_link);
            destroy_pd(pv_pd->cr3);
            if (pv_pd->user_cr3 != pv_pd->cr3) {
                destroy_pd(pv_pd->user_cr3);
            }
            node = node->next;
            sfree(pv_pd, sizeof(pv_pd_t));
        } while (node != end);
    }
    sfree(pv, sizeof(pv_t));
}

void pv_die(char* reason) {
    thread_t* t = get_current();
    sim_printf("PV kernel %d killed: %s", t->rb_node.key, reason);
    printf("PV kernel %d killed: %s\n", t->rb_node.key, reason);
    t->process->exit_value = GUEST_CRASH_STATUS;
    kill_current();
}

void pv_switch_mode(process_t* p, int kernelmode) {
    pv_pd_t* pv_pd = p->pv->active_shadow_pd;
    pa_t target_cr3 = (kernelmode != 0 ? pv_pd->cr3 : pv_pd->user_cr3);
    p->cr3 = target_cr3;
    set_cr3(target_cr3);
}

void pv_select_pd(process_t* p, pv_pd_t* pv_pd) {
    pv_t* pv = p->pv;
    pv_pd_t* old_pv_pd = pv->active_shadow_pd;
    pv->active_shadow_pd = pv_pd;
    pv_pd->refcount++;
    p->cr3 = pv_pd->cr3;
    set_cr3(p->cr3);
    old_pv_pd->refcount--;
    if (old_pv_pd->refcount == 0) {
        queue_detach(&pv->shadow_pds, &old_pv_pd->pv_link);
        destroy_pd(old_pv_pd->cr3);
        if (old_pv_pd->user_cr3 != old_pv_pd->cr3) {
            destroy_pd(old_pv_pd->user_cr3);
        }
        sfree(old_pv_pd, sizeof(pv_pd_t));
    }
}

void pv_handle_fault(ureg_t* frame, thread_t* t) {
    pv_t* pv = t->process->pv;
    pv_idt_entry_t* idt = pv_classify_interrupt(pv, frame->cause);
    if (idt->eip == 0) {
        goto no_idt_handler;
    }
    reg_t new_esp;
    pv_frame_t pv_f;
    pv_f.cr2 =
        (frame->cause == SWEXN_CAUSE_PAGEFAULT ? frame->cr2 - USER_MEM_START
                                               : 0);
    pv_f.error_code = frame->error_code;
    pv_f.eip = frame->eip;
    pv_f.eflags =
        (pv->vif != 0 ? (frame->eflags | EFL_IF) : (frame->eflags & (~EFL_IF)));
    if (frame->eip >= USER_MEM_START) {
        pv_switch_mode(t->process, 1);
        reg_t esp0 = pv->vesp0;
        esp0 = (esp0 & (~(sizeof(va_t) - 1)));
        reg_t esp = frame->esp;
        esp0 -= sizeof(reg_t);
        if (copy_to_user(esp0, sizeof(reg_t), &esp) != 0) {
            goto push_frame_fail;
        }
        pv_f.vcs = GUEST_INTERRUPT_UMODE;
        esp0 -= sizeof(pv_frame_t);
        if (copy_to_user(esp0, sizeof(pv_frame_t), &pv_f) != 0) {
            goto push_frame_fail;
        }
        new_esp = esp0;
    } else {
        new_esp = frame->esp;
        pv_f.vcs = GUEST_INTERRUPT_KMODE;
        new_esp -= sizeof(pv_frame_t);
        if (copy_to_user(new_esp, sizeof(pv_frame_t), &pv_f) != 0) {
            goto push_frame_fail;
        }
    }
    frame->eip = idt->eip;
    frame->esp = new_esp;
    return;

push_frame_fail:
    pv_die("Error when pushing interrupt frame to stack");
no_idt_handler:
    pv_die("No interrupt handler installed");
}

static inline void do_inject_irq(thread_t* t,
                                 pv_t* pv,
                                 stack_frame_t* f,
                                 int arg,
                                 va_t eip) {
    reg_t new_esp;
    pv_frame_t pv_f;
    pv_f.cr2 = 0;
    pv_f.error_code = arg;
    pv_f.eip = f->eip;
    pv_f.eflags = f->eflags;
    if (f->eip >= USER_MEM_START) {
        pv_switch_mode(t->process, 1);
        reg_t esp0 = pv->vesp0;
        esp0 = (esp0 & (~(sizeof(va_t) - 1)));
        reg_t esp = f->esp;
        esp0 -= sizeof(reg_t);
        if (copy_to_user(esp0, sizeof(reg_t), &esp) != 0) {
            goto push_frame_fail;
        }
        pv_f.vcs = GUEST_INTERRUPT_UMODE;
        esp0 -= sizeof(pv_frame_t);
        if (copy_to_user(esp0, sizeof(pv_frame_t), &pv_f) != 0) {
            goto push_frame_fail;
        }
        new_esp = esp0;
    } else {
        new_esp = f->esp;
        pv_f.vcs = GUEST_INTERRUPT_KMODE;
        new_esp -= sizeof(pv_frame_t);
        if (copy_to_user(new_esp, sizeof(pv_frame_t), &pv_f) != 0) {
            goto push_frame_fail;
        }
    }
    pv_mask_interrupt(pv);
    f->eip = eip;
    f->esp = new_esp;
    return;

push_frame_fail:
    pv_die("Error when pushing interrupt frame to stack");
}

int pv_inject_irq(stack_frame_t* f, int index, int arg) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    if (pv == NULL) {
        return -1;
    }
    if (f->cs != SEGSEL_PV_CS || (pv->vif & EFL_IF) == 0) {
        pv_irq_t* irq = &pv->vidt.pending_irq[index - PV_IRQ_START];
        irq->pending = 1;
        irq->arg = arg;
        return 0;
    }
    pv_idt_entry_t* idt = &pv->vidt.irq[index - PV_IRQ_START];
    if (idt->eip == 0) {
        goto no_idt_handler;
    }
    do_inject_irq(t, pv, f, arg, idt->eip);
    return 0;

no_idt_handler:
    pv_die("No interrupt handler installed");
    return -1;
}

void pv_check_pending_irq(stack_frame_t* f) {
    thread_t* t = get_current();
    pv_t* pv = t->process->pv;
    if (pv == NULL || f->cs != SEGSEL_PV_CS || (pv->vif & EFL_IF) == 0) {
        return;
    }
    pv_idt_t* vidt = &pv->vidt;
    int old_if = save_clear_if();
    int i;
    for (i = 0; i < (PV_IRQ_END - PV_IRQ_START); i++) {
        if (vidt->pending_irq[i].pending != 0) {
            if (vidt->irq[i].eip == 0) {
                goto no_idt_handler;
            }
            do_inject_irq(t, pv, f, vidt->pending_irq[i].arg, vidt->irq[i].eip);
            vidt->pending_irq[i].pending = 0;
            restore_if(old_if);
            return;
        }
    }
    restore_if(old_if);
    return;

no_idt_handler:
    pv_die("No interrupt handler installed");
}
