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

static int create_boot_pd(process_t* p, pa_t bootmem, int n_pages);

void pv_init() {
    uint64_t* gdt = (uint64_t*)gdt_base();
    /* copy cs's flags so we do not need to create one */
    uint64_t cs_flags = (gdt[SEGSEL_USER_CS_IDX] & GDT_FLAG_MASK);
    /* copy ds's flags so we do not need to create one */
    uint64_t ds_flags = (gdt[SEGSEL_USER_DS_IDX] & GDT_FLAG_MASK);
    uint64_t fs_flags = (ds_flags & (~GDT_G_BIT));
    uint64_t base = USER_MEM_START;
    uint64_t limit = (PV_VM_LIMIT / PAGE_SIZE) - 1;
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
        /* do not set P bit for ZFOD */
        (*pt)[pt_index] = make_pte(bootmem + offset, 0, PTE_USER, PTE_RW, 0);
        restore_if(old_if);
        invlpg(m_start + offset);
    } while (++i < n_pages);
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
    if (add_region(p, USER_MEM_START, n_bootmem_pages, bootmem, 1) != 0) {
        goto alloc_region_fail;
    }
    if (create_boot_pd(p, bootmem, n_bootmem_pages) != 0) {
        goto create_boot_pd_fail;
    }
    /* temporarily use new process's cr3 to load elf */
    pa_t old_cr3 = get_current()->process->cr3;
    get_current()->process->cr3 = p->cr3;
    set_cr3(p->cr3);

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
    get_current()->process->cr3 = old_cr3;
    set_cr3(old_cr3);
create_boot_pd_fail:
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
            pv_pd_t* shadow_pd = queue_data(node, pv_pd_t, pv_link);
            destroy_pd(shadow_pd->cr3);
            if (shadow_pd->user_cr3 != shadow_pd->cr3) {
                destroy_pd(shadow_pd->user_cr3);
            }
            node = node->next;
            sfree(shadow_pd, sizeof(pv_pd_t));
        } while (node != end);
    }
    sfree(pv, sizeof(pv_t));
}
