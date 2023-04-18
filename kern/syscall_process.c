/** @file syscall_process.c
 *
 *  @brief process management syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <stdio.h>
#include <string.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <assert.h>
#include <loader.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>
#include <usermem.h>

/**
 * @brief gettid() syscall handler
 * @param f saved regs
 */
void sys_gettid_real(stack_frame_t* f) {
    f->eax = (reg_t)get_current()->rb_node.key;
}

/**
 * @brief copy a region of current process to a process
 * @param p target process
 * @param src source region
 * @return 0 on success, or -1 on failure
 */
static int copy_region(process_t* p, region_t* src);

/**
 * @brief fork() syscall handler
 * @param f saved regs
 */
void sys_fork_real(stack_frame_t* f) {
    thread_t* current = get_current();
    process_t* p = current->process;
    mutex_lock(&p->refcount_lock);
    if (p->refcount != 1) {
        /* reject multithread fork() */
        mutex_unlock(&p->refcount_lock);
        goto fork_multiple_threads;
    }
    mutex_unlock(&p->refcount_lock);
    thread_t* t = create_empty_process();
    if (t == NULL) {
        goto create_thread_fail;
    }
    int tid = alloc_tid();
    t->process->pid = t->rb_node.key = tid;
    int i, n = vector_size(&p->regions);
    for (i = 0; i < n; i++) {
        int result =
            copy_region(t->process, (region_t*)vector_at(&p->regions, i));
        if (result != 0) {
            goto copy_region_fail;
        }
    }
    t->esp3 = current->esp3;
    t->eip3 = current->eip3;
    t->swexn_arg = current->swexn_arg;
    t->eip0 = current->eip0;

    t->kernel_esp -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)t->kernel_esp;
    *frame = *f;
    frame->eax = 0; /* child process will get 0 from fork() */

    t->kernel_esp -= sizeof(yield_frame_t);
    yield_frame_t* yf = (yield_frame_t*)t->kernel_esp;
    yf->eflags = DEFAULT_EFLAGS;
    yf->raddr = (reg_t)return_to_user;

    t->process->parent = p;
    mutex_lock(&p->wait_lock);
    queue_insert_head(&p->live_childs, &t->process->sible_link);
    p->nchilds++;
    mutex_unlock(&p->wait_lock);
    add_thread(t);
    int old_if = spl_lock(&ready_lock);
    insert_ready_tail(t);
    spl_unlock(&ready_lock, old_if);
    f->eax = (reg_t)tid;
    return;

copy_region_fail:
    destroy_thread(t);
create_thread_fail:
    f->eax = (reg_t)-1;
    return;

fork_multiple_threads:
    f->eax = (reg_t)-2;
    return;
}

static int copy_region(process_t* p, region_t* src) {
    process_t* cur_p = get_current()->process;
    int n_pages = src->size / PAGE_SIZE;
    pa_t paddr = alloc_user_pages(n_pages);
    if (paddr == 0) {
        goto alloc_segment_fail;
    }
    if (add_region(p, src->addr, n_pages, paddr, src->is_rw) != 0) {
        goto add_region_fail;
    }

    int rw = (src->is_rw ? PTE_RW : PTE_RO);
    int i = 0;
    int offset = 0;
    /* file page table one entry per loop */
    pa_t dst_pt_pa = find_or_create_pt(p, src->addr);
    if (dst_pt_pa == BAD_PA) {
        goto add_pt_fail;
    }
    pa_t src_pt_pa = find_or_create_pt(cur_p, src->addr);
    do {
        offset = i * PAGE_SIZE;
        /* step to next page table */
        int pt_index = get_pt_index(src->addr + offset);
        if (pt_index == 0) {
            dst_pt_pa = find_or_create_pt(p, src->addr + offset);
            if (dst_pt_pa == BAD_PA) {
                goto add_pt_fail;
            }
            src_pt_pa = find_or_create_pt(cur_p, src->addr);
        }

        int old_if = save_clear_if();
        /* check if source page is never accessed */
        page_table_t* src_pt = (page_table_t*)map_phys_page(src_pt_pa, NULL);
        if (((*src_pt)[pt_index] & (PTE_PRESENT << PTE_P_SHIFT)) != 0) {
            /* need to copy non zero page */
            page_table_t* dst_pt =
                (page_table_t*)map_phys_page(dst_pt_pa, NULL);
            (*dst_pt)[pt_index] =
                make_pte(paddr + offset, 0, PTE_USER, rw, PTE_PRESENT);
            char* page = (char*)map_phys_page(paddr + offset, NULL);
            memcpy(page, (char*)(src->addr + offset), PAGE_SIZE);
        } else {
            /* source page is waiting for ZFOD, so dest page will also ZFOD */
            page_table_t* dst_pt =
                (page_table_t*)map_phys_page(dst_pt_pa, NULL);
            (*dst_pt)[pt_index] = make_pte(paddr + offset, 0, PTE_USER, rw, 0);
        }
        restore_if(old_if);
    } while (++i < n_pages);
    return 0;

add_pt_fail:
    /* page tables are reachable from page directory so no need to free them */
    vector_pop(&p->regions);
add_region_fail:
    free_user_pages(paddr, n_pages);
alloc_segment_fail:
    return -1;
}

/**
 * @brief task_vanish() syscall handler
 * @param f saved regs
 */
void sys_task_vanish_real(stack_frame_t* f) {
    process_t* p = get_current()->process;
    p->exit_value = (int)f->esi;
    mutex_lock(&p->refcount_lock);
    queue_t *node = p->threads, *end = p->threads;
    do {
        /* send a pending exit to all other threads,
         * blocked threads will exit when the thread blocking it exits
         * sleeping threads will exit when timer is reached
         * threads waiting for input will exit after getting input
         */
        thread_t* t = queue_data(node, thread_t, process_link);
        t->pending_exit = 1;
        int old_if = spl_lock(&t->status_lock);
        /* wakeup descheduled threads */
        if (t->status == THREAD_DESCHEDULED) {
            int old_if2 = spl_lock(&ready_lock);
            insert_ready_tail(t);
            spl_unlock(&ready_lock, old_if2);
        }
        spl_unlock(&t->status_lock, old_if);
        node = node->next;
    } while (node != end);
    mutex_unlock(&p->refcount_lock);
    kill_current();
}

/**
 * @brief set_status() syscall handler
 * @param f saved regs
 */
void sys_set_status_real(stack_frame_t* f) {
    get_current()->process->exit_value = (int)f->esi;
}

/**
 * @brief exec() syscall handler
 * @param f saved regs
 */
void sys_exec_real(stack_frame_t* f) {
    thread_t* current = get_current();
    process_t* p = current->process;
    mutex_unlock(&p->refcount_lock);
    if (p->refcount != 1) {
        /* reject multithread exec() */
        mutex_unlock(&p->refcount_lock);
        goto exec_multiple_threads;
    }
    mutex_unlock(&p->refcount_lock);
    reg_t esi = f->esi;
    va_t pexe;
    if (copy_from_user((va_t)esi, sizeof(va_t), &pexe) != 0) {
        goto read_exe_fail;
    }
    char* exe = copy_string_from_user(pexe, MAX_EXECNAME_LEN);
    if (exe == NULL) {
        goto read_exe_fail;
    }
    va_t pargv;
    if (copy_from_user((va_t)esi + sizeof(va_t), sizeof(va_t), &pargv) != 0) {
        goto read_argv_fail;
    }
    int argc = 0;
    while (1) {
        if (argc > MAX_NUM_ARG) {
            goto read_argv_fail;
        }
        va_t argv;
        if (copy_from_user(pargv + argc * sizeof(va_t), sizeof(va_t), &argv) !=
            0) {
            goto read_argv_fail;
        }
        if (argv == 0) {
            break;
        }
        argc++;
    }
    char** argv_buf = smalloc(argc * sizeof(char*));
    if (argv_buf == NULL) {
        goto read_argv_fail;
    }
    int i;
    for (i = 0; i < argc; i++) {
        argv_buf[i] = copy_string_from_user(((va_t*)pargv)[i], MAX_ARG_LEN);
        if (argv_buf[i] == NULL) {
            goto read_argv_buf_fail;
        }
    }

    /* we create a new process and swap to it so we can recover if process
     * creation fails
     */
    thread_t* t =
        create_process(current->rb_node.key, exe, argc, (const char**)argv_buf);
    if (t == NULL) {
        goto create_process_fail;
    }
    for (i = argc - 1; i >= 0; i--) {
        free(argv_buf[i]);
    }
    sfree(argv_buf, argc * sizeof(char*));
    free(exe);
    swap_process_inplace(t);
    int old_if = spl_lock(&ready_lock);
    insert_ready_tail(t);
    spl_unlock(&ready_lock, old_if);
    kill_current();

create_process_fail:
read_argv_buf_fail:
    for (i--; i >= 0; i--) {
        free(argv_buf[i]);
    }
    sfree(argv_buf, argc * sizeof(char*));
read_argv_fail:
    free(exe);
read_exe_fail:
    f->eax = (reg_t)-1;
    return;

exec_multiple_threads:
    f->eax = (reg_t)-2;
    return;
}

/**
 * @brief wait() syscall handler
 * @param f saved regs
 */
void sys_wait_real(stack_frame_t* f) {
    /* it may be possible that pstatus is not valid when calling wait() and then
     * become valid after other thread allocates pages so we do not do check
     * now
     */
    va_t pstatus = f->esi;
    thread_t* current = get_current();
    process_t* p = current->process;
    /* it is possible that a process create multiple child processes and then
     * create multiple threads and then call wait() in these threads, so we need
     * to handle this case and count the waiting threads
     */
    mutex_lock(&p->wait_lock);
    if (p->nchilds <= p->nwaiters) {
        /* Pigeonhole principle prevents us to get a child in future even if
         * there are some childs
         */
        mutex_unlock(&p->wait_lock);
        goto no_child_to_wait;
    }
    p->nwaiters++;
    while (p->dead_childs == NULL) {
        cv_wait(&p->wait_cv, &p->wait_lock);
    }
    process_t* child = queue_data(p->dead_childs, process_t, sible_link);
    /* copy the exit status first so we do not need to put the child back to the
     * queue if copy fails
     */
    if (pstatus != 0 &&
        copy_to_user(pstatus, sizeof(int), &child->exit_value) != 0) {
        p->nwaiters--;
        mutex_unlock(&p->wait_lock);
        goto bad_pstatus;
    }
    queue_remove_head(&p->dead_childs);
    p->nchilds--;
    p->nwaiters--;
    mutex_unlock(&p->wait_lock);
    f->eax = (reg_t)child->pid;
    sfree(child, sizeof(process_t));
    return;

bad_pstatus:
    f->eax = (reg_t)-1;
    return;

no_child_to_wait:
    f->eax = (reg_t)-2;
    return;
}

/**
 * @brief sleep() syscall handler
 * @param f saved regs
 */
void sys_sleep_real(stack_frame_t* f) {
    int dt = (int)f->esi;
    if (dt <= 0) {
        f->eax = (reg_t)dt;
        return;
    }
    thread_t* current = get_current();
    heap_node_t node;
    node.key = ticks + dt;
    node.value = (void*)current;
    int old_if = spl_lock(&timer_lock);
    if (heap_insert(&timers, &node) != 0) {
        spl_unlock(&timer_lock, old_if);
        f->eax = (reg_t)-2;
        return;
    }
    int old_if2 = spl_lock(&ready_lock);
    current->status = THREAD_SLEEPING;
    thread_t* t = select_next();
    spl_unlock(&ready_lock, old_if2);
    yield_to_spl_unlock(t, &timer_lock, old_if);
    f->eax = 0;
    return;
}

/**
 * @brief get_ticks() syscall handler
 * @param f saved regs
 */
void sys_get_ticks_real(stack_frame_t* f) {
    f->eax = (reg_t)ticks;
    return;
}
