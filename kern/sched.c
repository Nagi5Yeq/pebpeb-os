/** @file sched.c
 *
 *  @brief scheduling functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <limits.h>
#include <malloc_internal.h>
#include <string.h>
#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <loader.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <pts.h>
#include <pv.h>
#include <sched.h>
#include <sync.h>

/* .text, .rodata, .data+bss, stack and a heap and one for future new_pages */
#define INIT_NUM_REGIONS 6

/**
 * @brief load a elf to memory space
 * @param p process
 * @param elf elf header
 * @param exe elf file name
 * @return 0 on success, -1 on failure
 */
static int process_load_elf(process_t* p, simple_elf_t* elf, char* exe);
/**
 * @brief load a elf segment to memory space
 * @param p process
 * @param f elf file entry, NULL for empty segments
 * @param offset offset in elf file
 * @param f_len length of data in elf file
 * @param vaddr addr to load to
 * @param m_len length of segment
 * @param is_rw is segment writable
 * @return 0 on success, -1 on failure
 */
static int load_segment(process_t* p,
                        file_t* f,
                        int offset,
                        int f_len,
                        va_t vaddr,
                        int m_len,
                        int is_rw);

process_t* init_process;

spl_t ready_lock = SPL_INIT;
queue_t* ready = NULL;

rb_t* threads = &rb_nil;
mutex_t threads_lock = MUTEX_INIT;

uint64_t create_segsel(va_t base, va_size_t limit, uint64_t flags) {
    uint64_t b = (uint64_t)base;
    uint64_t l = (uint64_t)limit;
    uint64_t ss = (((b & GDT_BASE_MASK_HI) << GDT_BASE_SHIFT_HI) |
                   ((b & GDT_BASE_MASK_LO) << GDT_BASE_SHIFT_LO) |
                   ((l & GDT_LIMIT_MASK_HI) << GDT_LIMIT_SHIFT_HI) |
                   ((l & GDT_LIMIT_MASK_LO) << GDT_LIMIT_SHIFT_LO) | flags);
    return ss;
}

void setup_percpu(percpu_t* percpu) {
    uint64_t* gdt = (uint64_t*)gdt_base();
    uint64_t ds = gdt[SEGSEL_KERNEL_DS_IDX];
    /* copy ds's flags so we do not need to create one */
    uint64_t ds_flags = (ds & GDT_FLAG_MASK & (~GDT_G_BIT));
    gdt[SEGSEL_KERNEL_FS_IDX] =
        create_segsel((va_t)percpu, sizeof(percpu_t) - 1, ds_flags);
    set_fs(SEGSEL_KERNEL_FS);
}

void setup_kth(thread_t* kthread, process_t* kprocess) {
    memset(kthread, 0, sizeof(thread_t));
    memset(kprocess, 0, sizeof(process_t));
    kprocess->refcount = 1;
    kprocess->cr3 = (pa_t)kernel_pd;
    kthread->process = kprocess;
    kthread->pts = active_pts;
    set_current(kthread);
    set_kthread(kthread);
}

/* exit value when all tasks vanish and status is never set, no religious
 * meaning
 */
#define DEFAULT_EXIT_VALUE 666

thread_t* create_empty_process() {
    process_t* p = smalloc(sizeof(process_t));
    if (p == NULL) {
        goto alloc_pcb_fail;
    }
    thread_t* t = smalloc(sizeof(thread_t));
    if (t == NULL) {
        goto alloc_tcb_fail;
    }
    t->stack = smalloc(K_STACK_SIZE);
    if (t->stack == NULL) {
        goto alloc_thread_stack_fail;
    }
    p->exit_value = DEFAULT_EXIT_VALUE;
    p->parent = NULL;
    p->refcount_lock = MUTEX_INIT;
    p->refcount = 1;
    p->threads = NULL;
    p->nchilds = 0;
    p->live_childs = NULL;
    p->dead_childs = NULL;
    p->nwaiters = 0;
    p->wait_lock = MUTEX_INIT;
    p->wait_cv = CV_INIT;
    if (vector_init(&p->regions, sizeof(region_t), INIT_NUM_REGIONS) != 0) {
        goto alloc_region_fail;
    }

    p->cr3 = alloc_user_pages(1);
    if (p->cr3 == 0) {
        goto alloc_pd_fail;
    }
    int old_if = save_clear_if();
    page_directory_t* pd = (page_directory_t*)map_phys_page(p->cr3, NULL);
    memset(pd, 0, PAGE_SIZE);
    int i;
    /* copy kernel direct mapping */
    for (i = 0; i < USER_PD_START; i++) {
        (*pd)[i] = (*kernel_pd)[i];
    }
    restore_if(old_if);
    p->mm_lock = MUTEX_INIT;
    p->pv = NULL;

    t->status = THREAD_DEAD;
    t->status_lock = SPL_INIT;
    t->pending_exit = 0;
    queue_insert_head(&p->threads, &t->process_link);
    t->rb_node.parent = NULL; /* mark that the thread is not added to rbtree */
    t->pts = get_current()->pts;
    mutex_lock(&t->pts->lock);
    t->pts->refcount++;
    mutex_unlock(&t->pts->lock);
    t->process = p;
    t->eip3 = 0;
    t->esp0 = t->kernel_esp = (reg_t)&t->stack[K_STACK_SIZE];
    t->eip0 = 0;
    t->df3 = 0;
    return t;

alloc_pd_fail:
    vector_free(&p->regions);
alloc_region_fail:
    sfree(t->stack, K_STACK_SIZE);
alloc_thread_stack_fail:
    sfree(t, sizeof(thread_t));
alloc_tcb_fail:
    sfree(p, sizeof(process_t));
alloc_pcb_fail:
    return NULL;
}

static int process_load_elf(process_t* p, simple_elf_t* elf, char* exe) {
    file_t* f = find_file(exe);

    if (elf->e_txtlen != 0 &&
        load_segment(p, f, elf->e_txtoff, elf->e_txtlen, elf->e_txtstart,
                     elf->e_txtlen, 0) != 0) {
        goto load_segment_fail;
    }
    if (elf->e_rodatlen != 0 &&
        load_segment(p, f, elf->e_rodatoff, elf->e_rodatlen, elf->e_rodatstart,
                     elf->e_rodatlen, 0) != 0) {
        goto load_segment_fail;
    }
    if (elf->e_datlen != 0 &&
        load_segment(p, f, elf->e_datoff, elf->e_datlen, elf->e_datstart,
                     elf->e_datlen, 1) != 0) {
        goto load_segment_fail;
    }
    /* if data segment is loaded, try to load bss after data */
    if (elf->e_datlen != 0) {
        if (elf->e_bsslen != 0) {
            /* allow bss to start from unaligned/overlapped address */
            va_t bss_start = (elf->e_bssstart & PAGE_BASE_MASK);
            va_t bss_end = ((elf->e_bssstart + elf->e_bsslen + PAGE_SIZE) &
                            PAGE_BASE_MASK);
            va_t data_start = (elf->e_datstart & PAGE_BASE_MASK);
            va_t data_end = ((elf->e_datstart + elf->e_datlen + PAGE_SIZE) &
                             PAGE_BASE_MASK);
            if (bss_start >= data_start && bss_start < data_end) {
                /* append bss after data */
                bss_start = data_end;
            }
            /* let add_region handle overlap/overflow problem */
            if ((bss_end > bss_start) &&
                load_segment(p, NULL, 0, 0, bss_start, bss_end - bss_start,
                             1) != 0) {
                goto load_segment_fail;
            };
        }
    } else {
        if (elf->e_bsslen != 0 && load_segment(p, NULL, 0, 0, elf->e_bssstart,
                                               elf->e_bsslen, 1) != 0) {
            goto load_segment_fail;
        }
    }

    if (load_segment(p, NULL, 0, 0, DEFAULT_STACK_POS, DEFAULT_STACK_SIZE, 1)) {
        goto load_segment_fail;
    }
    return 0;

load_segment_fail:
    return -1;
}

thread_t* create_process(int tid, char* exe, int argc, const char** argv) {
    thread_t* t = create_empty_process();
    if (t == NULL) {
        goto alloc_tcb_fail;
    }
    t->process->pid = t->rb_node.key = ((tid == 0) ? alloc_tid() : tid);

    simple_elf_t elf;
    if (elf_load_helper(&elf, exe) != ELF_SUCCESS) {
        goto open_elf_fail;
    }

    if (elf.e_entry < USER_MEM_START) {
        va_size_t mem_size = PV_DEFAULT_SIZE;
        if (argc > 2) {
            goto too_many_args_for_pv;
        }
        if (argc > 1) {
            mem_size = (va_size_t)strtoul(argv[1], NULL, 10);
            if (mem_size < PV_MINIMUM_SIZE || mem_size == ULONG_MAX) {
                goto bad_mem_size_for_pv;
            }
        }
        mem_size <<= 20;
        return create_pv_process(t, &elf, exe, mem_size);
    }

    /* temporarily use new process's cr3 to load elf */
    pa_t old_cr3 = get_current()->process->cr3;
    get_current()->process->cr3 = t->process->cr3;
    set_cr3(t->process->cr3);

    if (process_load_elf(t->process, &elf, exe) != 0) {
        goto load_elf_fail;
    }

    unsigned long* argv_addrs = smalloc(argc * sizeof(unsigned long));
    if (argv_addrs == NULL) {
        goto alloc_argc_addr_fail;
    }
    char* esp = (char*)(DEFAULT_STACK_END);
    int i, len, total = 0;
    for (i = argc - 1; i >= 0; i--) {
        len = strlen(argv[i]);
        if (len + total > MAX_TOTAL_ARG_LEN) {
            goto load_elf_fail;
        }
        esp -= (len + 1);
        memcpy(esp, argv[i], len + 1);
        argv_addrs[i] = (unsigned long)esp;
    }
    unsigned long* new_esp =
        (unsigned long*)(((va_t)esp) & (~(sizeof(va_t) - 1)));
    --new_esp;
    *new_esp = 0;
    for (i = argc - 1; i >= 0; i--) {
        --new_esp;
        *new_esp = argv_addrs[i];
    }
    unsigned long new_argv = (unsigned long)new_esp;
    sfree(argv_addrs, argc * sizeof(unsigned long));

    new_esp -= 5;
    new_esp[0] = 0;                 /* return address */
    new_esp[1] = argc;              /* argc */
    new_esp[2] = new_argv;          /* argv */
    new_esp[3] = DEFAULT_STACK_END; /* stack_hi */
    new_esp[4] = DEFAULT_STACK_POS; /* stack_lo */

    get_current()->process->cr3 = old_cr3;
    set_cr3(old_cr3);

    t->kernel_esp -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)t->kernel_esp;
    frame->eip = (reg_t)elf.e_entry;
    frame->cs = SEGSEL_USER_CS;
    frame->eflags = DEFAULT_EFLAGS;
    frame->esp = (reg_t)new_esp;
    frame->ss = SEGSEL_USER_DS;
    frame->eax = 0;
    frame->ecx = 0;
    frame->edx = 0;
    frame->ebx = 0;
    frame->ebp = 0;
    frame->esi = 0;
    frame->edi = 0;
    frame->ds = SEGSEL_USER_DS;
    frame->es = SEGSEL_USER_DS;
    frame->fs = SEGSEL_USER_DS;
    frame->gs = SEGSEL_USER_DS;

    t->kernel_esp -= sizeof(yield_frame_t);
    yield_frame_t* yf = (yield_frame_t*)t->kernel_esp;
    yf->eflags = DEFAULT_EFLAGS;
    yf->raddr = (reg_t)return_to_user;
    return t;

alloc_argc_addr_fail:
load_elf_fail:
    get_current()->process->cr3 = old_cr3;
    set_cr3(old_cr3);
bad_mem_size_for_pv:
too_many_args_for_pv:
open_elf_fail:
    destroy_thread(t);
alloc_tcb_fail:
    return NULL;
}

void destroy_thread(thread_t* t) {
    process_t* p = t->process;
    mutex_lock(&t->pts->lock);
    t->pts->refcount--;
    if (p->pv != NULL) {
        queue_detach(&t->pts->pvs, &p->pv->pts_link);
    }
    mutex_unlock(&t->pts->lock);
    sfree(t->stack, K_STACK_SIZE);
    sfree(t, sizeof(thread_t));
    mutex_lock(&p->refcount_lock);
    queue_detach(&p->threads, &t->process_link);
    p->refcount--;
    if (p->refcount != 0) {
        mutex_unlock(&p->refcount_lock);
        return;
    }
    mutex_unlock(&p->refcount_lock);
    int i, n = vector_size(&p->regions);
    for (i = 0; i < n; i++) {
        region_t* r = (region_t*)vector_at(&p->regions, i);
        free_user_pages(r->paddr, r->size / PAGE_SIZE);
    }
    vector_free(&p->regions);
    if (p->pv == NULL) {
        destroy_pd(p->cr3);
    } else {
        destroy_pv(p->pv);
    }
    sfree(p, sizeof(process_t));
}

void destroy_pd(pa_t pd_pa) {
    int i;
    int old_if = save_clear_if();
    for (i = USER_PD_START; i < NUM_PAGE_ENTRY; i++) {
        page_directory_t* pd = (page_directory_t*)map_phys_page(pd_pa, NULL);
        if ((*pd)[i] != BAD_PDE) {
            pa_t pt = get_page_table((*pd)[i]);
            free_user_pages(pt, 1);
        }
    }
    restore_if(old_if);
    free_user_pages(pd_pa, 1);
}

int alloc_tid() {
    static int tid = IDLE_PID + 1;
    static int next_check = (1 << 31);
    if (tid == next_check) {
        /* allocated all pids */
        mutex_lock(&threads_lock);
        rb_t *p, *q;
        p = rb_min(threads);
        if (p == &rb_nil) {
            /* no pid inuse */
            tid = IDLE_PID + 1;
            next_check = (1 << 31);
        } else {
            while (1) {
                q = rb_next(p);
                if (q == &rb_nil) {
                    /* available tids are p+1 ~ 0x7fffffff */
                    if (p->key + 1 == (1 << 31)) {
                        /* all pids inuse */
                        panic("too many threads");
                    }
                    tid = p->key + 1;
                    next_check = (1 << 31);
                    break;
                }
                /* check until q-p>1, which gives us some free tids */
                if (q->key == p->key + 1) {
                    p = q;
                    continue;
                }
                tid = p->key + 1;
                next_check = q->key;
                break;
            }
        }
        mutex_unlock(&threads_lock);
    }
    return tid++;
}

static int load_segment(process_t* p,
                        file_t* f,
                        int f_off,
                        int f_len,
                        va_t m_off,
                        int m_len,
                        int is_rw) {
    va_t m_start = (m_off & PAGE_BASE_MASK);
    va_t m_end = ((m_off + m_len + (PAGE_SIZE - 1)) & PAGE_BASE_MASK);
    int n_pages = (m_end - m_start) / PAGE_SIZE;
    pa_t paddr = alloc_user_pages(n_pages);
    if (paddr == 0) {
        goto alloc_segment_fail;
    }
    /* handle address check and overlapping issues */
    if (add_region(p, m_start, n_pages, paddr, is_rw) != 0) {
        goto add_region_fail;
    }

    int i = 0;
    int offset = 0;
    /* file page table one entry per loop */
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
        (*pt)[pt_index] = make_pte(paddr + offset, 0, PTE_USER, PTE_RW, 0);
        restore_if(old_if);
        invlpg(m_start + offset);
    } while (++i < n_pages);

    if (f != NULL) {
        read_file(f, f_off, f_len, (char*)m_off);
    }
    if (is_rw == 0) {
        i = 0;
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
            pte_t* pte = &(*pt)[pt_index];
            *pte = ((*pte) & (~(PTE_RW << PTE_RW_SHIFT)));
            restore_if(old_if);
            invlpg(m_start + offset);
        } while (++i < n_pages);
    }
    return 0;

add_pt_fail:
    /* page tables are reachable from page directory so no need to free them */
    vector_pop(&p->regions);
add_region_fail:
    free_user_pages(paddr, n_pages);
alloc_segment_fail:
    return -1;
}

pa_t find_or_create_pt(process_t* p, va_t vaddr) {
    int old_if = save_clear_if();
    page_directory_t* pd = (page_directory_t*)map_phys_page(p->cr3, NULL);
    pde_t* pde = &(*pd)[get_pd_index(vaddr)];
    if (*pde == BAD_PDE) {
        /* add a page table */
        pa_t pt = alloc_user_pages(1);
        if (pt == 0) {
            return 0;
        }
        va_t pt_addr = map_phys_page(pt, NULL);
        memset((void*)pt_addr, 0, PAGE_SIZE);

        map_phys_page(p->cr3, NULL);
        *pde = make_pde(pt, PTE_USER, PTE_RW, PTE_PRESENT);
        invlpg(vaddr);
        restore_if(old_if);
        return pt;
    }
    pa_t result = get_page_table(*pde);
    restore_if(old_if);
    return result;
}

int add_region(process_t* p, va_t start, int n_pages, pa_t pa, int is_rw) {
    if (start > DEFAULT_STACK_END || start < USER_MEM_START) {
        return -1;
    }
    va_t end = start + n_pages * PAGE_SIZE;
    if (end < start) {
        return -1;
    }
    int i, n = vector_size(&p->regions);
    for (i = 0; i < n; i++) {
        /* check overlapping */
        region_t* r = (region_t*)vector_at(&p->regions, i);
        va_t this_start = r->addr;
        va_t this_end = this_start + r->size;
        if (start >= this_start && start < this_end) {
            return -1;
        }
        if (end > this_start && end <= this_end) {
            return -1;
        }
        if (this_start >= start && this_start < end) {
            return -1;
        }
        if (this_end > start && this_end <= end) {
            return -1;
        }
    }
    region_t newr;
    newr.addr = start;
    newr.size = n_pages * PAGE_SIZE;
    newr.paddr = pa;
    newr.is_rw = is_rw;
    return vector_push(&p->regions, &newr);
}

thread_t* select_next() {
    thread_t* t;
    if (ready != NULL) {
        t = queue_data(queue_remove_head(&ready), thread_t, sched_link);
    } else {
        t = get_idle();
    }
    return t;
}

void insert_ready_tail(thread_t* t) {
    t->status = THREAD_READY;
    queue_insert_tail(&ready, &t->sched_link);
}

void insert_ready_head(thread_t* t) {
    t->status = THREAD_READY;
    queue_insert_head(&ready, &t->sched_link);
}

reg_t save_and_setup_env(thread_t* t, reg_t esp) {
    get_current()->kernel_esp = esp;
    set_current(t);
    t->status = THREAD_RUNNING;
    set_esp0(t->esp0);
    set_cr3(t->process->cr3);
    return t->kernel_esp;
}

void swap_process_inplace(thread_t* newt) {
    disable_interrupts();
    thread_t* oldt = get_current();
    process_t* oldp = oldt->process;
    process_t* newp = newt->process;
    pa_t cr3 = oldp->cr3;
    vector_t regions = oldp->regions;
    queue_t* p_threads = oldp->threads;
    pts_t* pts = oldt->pts;
    pv_t* pv = oldp->pv;
    oldp->cr3 = newp->cr3;
    oldp->regions = newp->regions;
    oldp->threads = newp->threads;
    oldp->pv = newp->pv;
    newp->cr3 = cr3;
    newp->regions = regions;
    newp->threads = p_threads;
    newp->pv = pv;
    oldt->process = newp;
    oldt->pts = newt->pts;
    newt->process = oldp;
    newt->pts = pts;
    set_cr3(newp->cr3);
    mutex_lock(&threads_lock);
    /* move oldt's rbtree node to newt */
    newt->rb_node = oldt->rb_node;
    if (threads == &oldt->rb_node) {
        threads = &newt->rb_node;
    };
    if (newt->rb_node.left != &rb_nil) {
        newt->rb_node.left->parent = &newt->rb_node;
    }
    if (newt->rb_node.right != &rb_nil) {
        newt->rb_node.right->parent = &newt->rb_node;
    }
    if (newt->rb_node.parent != &rb_nil) {
        if (newt->rb_node.parent->left == &oldt->rb_node) {
            newt->rb_node.parent->left = &newt->rb_node;
        } else {
            newt->rb_node.parent->right = &newt->rb_node;
        }
    }
    oldt->rb_node.parent = NULL;
    mutex_unlock(&threads_lock);
    enable_interrupts();
}

void kill_current() {
    thread_t* current = get_current();
    process_t* p = current->process;
    /* respawn important process */
    if (p == init_process) {
        if (p->refcount == 1) {
            mutex_lock(&p->wait_lock);
            /* reclaim dead childs */
            while (p->dead_childs != NULL) {
                queue_t* node = p->dead_childs;
                queue_detach(&p->dead_childs, node);
                process_t* child_process =
                    queue_data(node, process_t, sible_link);
                sfree(child_process, sizeof(process_t));
                p->nchilds--;
            }
            mutex_unlock(&p->wait_lock);
            const char* init_args[] = {INIT_NAME};
            thread_t* new_init =
                create_process(current->rb_node.key, INIT_NAME, 1, init_args);
            if (new_init == NULL) {
                panic("no space to allocate init process");
            }
            swap_process_inplace(new_init);
            int old_if = spl_lock(&ready_lock);
            insert_ready_tail(new_init);
            spl_unlock(&ready_lock, old_if);
        }
    }
    p = current->process;

    mutex_lock(&current->pts->lock);
    current->pts->refcount--;
    if (p->pv != NULL) {
        queue_detach(&current->pts->pvs, &p->pv->pts_link);
    }
    mutex_unlock(&current->pts->lock);

    if (current->rb_node.parent != NULL) {
        remove_thread(current);
    }

    mutex_lock(&p->refcount_lock);
    p->refcount--;
    queue_detach(&p->threads, &current->process_link);
    int is_last = (p->refcount == 0);
    mutex_unlock(&p->refcount_lock);
    if (is_last != 0) {
        /* move childs to init process */
        mutex_lock(&p->wait_lock);
        mutex_lock(&init_process->wait_lock);
        while (p->live_childs != NULL) {
            queue_t* node = p->live_childs;
            queue_detach(&p->live_childs, node);
            queue_insert_tail(&init_process->live_childs, node);
            process_t* child_process = queue_data(node, process_t, sible_link);
            child_process->parent = init_process;
            init_process->nchilds++;
        }
        while (p->dead_childs != NULL) {
            queue_t* node = p->dead_childs;
            queue_detach(&p->dead_childs, node);
            queue_insert_tail(&init_process->dead_childs, node);
            process_t* child_process = queue_data(node, process_t, sible_link);
            child_process->parent = init_process;
            init_process->nchilds++;
            cv_signal(&init_process->wait_cv);
        }
        mutex_unlock(&init_process->wait_lock);
        mutex_unlock(&p->wait_lock);

        /* free userspace memory */
        pa_t old_cr3 = p->cr3;
        p->cr3 = (pa_t)kernel_pd;
        set_cr3((pa_t)kernel_pd);
        int i, n = vector_size(&p->regions);
        for (i = 0; i < n; i++) {
            region_t* r = (region_t*)vector_at(&p->regions, i);
            free_user_pages(r->paddr, r->size / PAGE_SIZE);
        }
        vector_free(&p->regions);
        if (p->pv == NULL) {
            destroy_pd(old_cr3);
        } else {
            destroy_pv(p->pv);
        }
    }

    /* make sure we do not get swapped out before borrowing kthread's stack */
    mutex_lock(&malloc_lock);
    disable_interrupts();
    current->status = THREAD_DEAD;
    if (is_last != 0) {
        /* we cannot switch back after p is reclaimed so we must put cv_signal
         * after disable_interrupts
         */
        if (p->parent != NULL) {
            mutex_lock(&p->parent->wait_lock);
            queue_detach(&p->parent->live_childs, &p->sible_link);
            queue_insert_tail(&p->parent->dead_childs, &p->sible_link);
            cv_signal(&p->parent->wait_cv);
            mutex_unlock(&p->parent->wait_lock);
        } else {
            _sfree(p, sizeof(process_t));
        }
    }
    /* borrow kth's stack for freeing thread control block
     * we can make sure kthread is not running since we are the running thread
     * and kthread's IF is cleared so it will not be interrupted
     */
    thread_t* kth = get_kthread();
    set_current(kth);
    switch_stack_cleanup(kth->kernel_esp, current);
}

/**
 * @brief clean thread control block and its stack
 * @param t thread
 */
void cleanup_dead_thread(thread_t* t) {
    _sfree(t->stack, K_STACK_SIZE);
    _sfree(t, sizeof(thread_t));
    mutex_unlock(&malloc_lock);
    /* we will return to kthread's loop and it will yield
     * cannot do yield here since it increases stack
     */
}

/**
 * @brief check if a task_vanish is pending and if so, do vanish
 * @param f saved regs
 */
void check_pending_signals(stack_frame_t* f) {
    /* only vanish before returning to userspace, otherwise some kernel
     * structure may be jammed
     */
    thread_t* t = get_current();
    if (t->pending_exit != 0 && f->cs == SEGSEL_USER_CS) {
        kill_current();
    }
    pv_check_pending_irq(f);
}

thread_t* find_thread(int tid) {
    rb_t* result = rb_find(threads, tid);
    if (result == NULL) {
        return NULL;
    }
    return rb_data(result, thread_t, rb_node);
}

void add_thread(thread_t* t) {
    mutex_lock(&threads_lock);
    rb_insert(&threads, &t->rb_node);
    mutex_unlock(&threads_lock);
}

void remove_thread(thread_t* t) {
    mutex_lock(&threads_lock);
    rb_delete(&threads, &t->rb_node);
    mutex_unlock(&threads_lock);
}
