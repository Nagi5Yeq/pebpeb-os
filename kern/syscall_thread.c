/** @file syscall_thread.c
 *
 *  @brief thread management syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <string.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <simics.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <assert.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <usermem.h>

/**
 * @brief thread_fork() syscall handler
 * @param f saved regs
 */
void sys_thread_fork_real(stack_frame_t* f) {
    thread_t* current = get_current();
    process_t* p = current->process;
    thread_t* t = smalloc(sizeof(thread_t));
    if (t == NULL) {
        goto alloc_tcb_fail;
    }
    t->stack = smemalign(PAGE_SIZE, K_STACK_SIZE);
    if (t->stack == NULL) {
        goto alloc_thread_stack_fail;
    }
    int tid = alloc_tid();
    t->rb_node.key = tid;
    t->status = THREAD_DEAD;
    t->status_lock = SPL_INIT;
    t->pending_exit = current->pending_exit;
    t->esp3 = current->esp3;
    t->eip3 = current->eip3;
    t->df3 = current->df3;
    t->swexn_arg = current->swexn_arg;
    t->esp0 = t->kernel_esp = (reg_t)&t->stack[K_STACK_SIZE];
    t->eip0 = current->eip0;

    t->kernel_esp -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)t->kernel_esp;
    *frame = *f;
    frame->eax = 0;

    t->kernel_esp -= sizeof(yield_frame_t);
    yield_frame_t* yf = (yield_frame_t*)t->kernel_esp;
    yf->eflags = DEFAULT_EFLAGS;
    yf->raddr = (reg_t)return_to_user;

    t->pts = get_current()->pts;
    mutex_lock(&t->pts->lock);
    t->pts->refcount++;
    mutex_unlock(&t->pts->lock);
    t->process = p;
    mutex_lock(&p->refcount_lock);
    p->refcount++;
    queue_insert_tail(&p->threads, &t->process_link);
    mutex_unlock(&p->refcount_lock);
    add_thread(t);
    int old_if = spl_lock(&ready_lock);
    insert_ready_tail(t);
    spl_unlock(&ready_lock, old_if);
    f->eax = (reg_t)tid;
    return;

alloc_thread_stack_fail:
    sfree(t, sizeof(thread_t));
alloc_tcb_fail:
    f->eax = (reg_t)-1;
}

/**
 * @brief deschedule() syscall handler
 * @param f saved regs
 */
void sys_deschedule_real(stack_frame_t* f) {
    va_t preject = (va_t)f->esi;
    int reject;
    thread_t* current = get_current();
    int old_if = spl_lock(&current->status_lock);
    if (copy_from_user(preject, sizeof(int), &reject) != 0) {
        spl_unlock(&current->status_lock, old_if);
        f->eax = (reg_t)-1;
        return;
    }
    if (reject != 0 || current->pending_exit != 0) {
        spl_unlock(&current->status_lock, old_if);
        f->eax = (reg_t)0;
        return;
    }
    int old_if2 = spl_lock(&ready_lock);
    current->status = THREAD_DESCHEDULED;
    thread_t* t = select_next();
    spl_unlock(&ready_lock, old_if2);
    /* release status_lock last to make sure nobody make us runnable before
     * deschedule
     */
    yield_to_spl_unlock(t, &current->status_lock, old_if);
    f->eax = (reg_t)0;
    return;
}

/**
 * @brief make_runnable() syscall handler
 * @param f saved regs
 */
void sys_make_runnable_real(stack_frame_t* f) {
    int tid = (int)f->esi;
    mutex_lock(&threads_lock);
    thread_t* t = find_thread(tid);
    if (t == NULL) {
        mutex_unlock(&threads_lock);
        f->eax = (reg_t)-2;
        return;
    }
    int old_if = spl_lock(&t->status_lock);
    /* we must hold threads_lock before making sure t is descheduled, otherwise
     * if t is running/ready, t can be freed at any time
     */
    if (t->status != THREAD_DESCHEDULED) {
        spl_unlock(&t->status_lock, old_if);
        mutex_unlock(&threads_lock);
        f->eax = (reg_t)-3;
        return;
    }
    mutex_unlock(&threads_lock);
    /* must lock ready lock before setting status to READY otherwise yield() may
     * yield to it before we insert it into ready queue
     */
    int old_if2 = spl_lock(&ready_lock);
    t->status = THREAD_READY;
    insert_ready_tail(t);
    spl_unlock(&ready_lock, old_if2);
    spl_unlock(&t->status_lock, old_if);
    f->eax = (reg_t)0;
    return;
}

/**
 * @brief vanish() syscall handler
 * @param f saved regs
 */
void sys_vanish_real(stack_frame_t* f) {
    kill_current();
}

/**
 * @brief yield() syscall handler
 * @param f saved regs
 */
void sys_yield_real(stack_frame_t* f) {
    int tid = (int)f->esi;
    if (tid == -1) {
        int old_if = spl_lock(&ready_lock);
        insert_ready_tail(get_current());
        thread_t* t = select_next();
        yield_to_spl_unlock(t, &ready_lock, old_if);
        f->eax = (reg_t)0;
        return;
    }
    mutex_lock(&threads_lock);
    thread_t* t = find_thread(tid);
    if (t == NULL) {
        mutex_unlock(&threads_lock);
        f->eax = (reg_t)-2;
        return;
    }
    /* we must hold threads_lock and ready_lock before making sure t is ready
     * if we do not have threads_lock, t may be freed
     * if we do not have ready_lock, t may be turned from ready to running by
     * other processors
     */
    int old_if = spl_lock(&ready_lock);
    if (t->status == THREAD_RUNNING) {
        /* t is running or other processor, no need to yield */
        t = select_next();
    } else if (t->status == THREAD_READY) {
        queue_detach(&ready, &t->sched_link);
    } else {
        spl_unlock(&ready_lock, old_if);
        mutex_unlock(&threads_lock);
        f->eax = (reg_t)-1;
        return;
    }
    /* if we are sure t is ready and remove it from ready queue, it will not
     * suddenly run so we can make sure t will not be freed
     */
    mutex_unlock(&threads_lock);
    insert_ready_tail(get_current());
    yield_to_spl_unlock(t, &ready_lock, old_if);
    f->eax = (reg_t)0;
    return;
}

/* eflags fields that are allowed to be changed by user */
#define EFLAGS_USER_MASK                                                     \
    (EFL_CF | EFL_PF | EFL_AF | EFL_ZF | EFL_SF | EFL_TF | EFL_DF | EFL_OF | \
     EFL_RF)

/**
 * @brief swexn() syscall handler
 * @param f saved regs
 */
void sys_swexn_real(stack_frame_t* f) {
    thread_t* current = get_current();
    reg_t args[4]; /* esp3, eip3, arg, ureg */
    reg_t esi = f->esi;
    if (copy_from_user(esi, 4 * sizeof(reg_t), args) != 0) {
        goto read_arg_fail;
    }
    reg_t esp3 = args[0], eip3 = args[1], pureg = args[3];
    if (esp3 != 0 && eip3 != 0) {
        /* for unlcear "stack end" definition */
        esp3 = (esp3 & (~(sizeof(va_t) - 1)));
        /* check first, will register after successful ureg copy */
        if (eip3 < USER_MEM_START || eip3 >= DEFAULT_STACK_END ||
            esp3 < USER_MEM_START || esp3 >= DEFAULT_STACK_END) {
            goto bad_user_entry;
        }
    }
    if (pureg != 0) {
        ureg_t ureg;
        if (copy_from_user((va_t)pureg, sizeof(ureg_t), &ureg) != 0) {
            goto bad_user_entry;
        }
        /* segment regs are ignored since they cannot be changed */
        /* only user fields in eflags are allowed to be changed */
        if ((ureg.eflags & (~EFLAGS_USER_MASK)) != DEFAULT_EFLAGS) {
            goto bad_user_entry;
        }
        f->eip = ureg.eip;
        f->eflags = ureg.eflags;
        f->esp = ureg.esp;
        f->eax = ureg.eax;
        f->ecx = ureg.ecx;
        f->edx = ureg.edx;
        f->ebx = ureg.ebx;
        f->ebp = ureg.ebp;
        f->esi = ureg.esi;
        f->edi = ureg.edi;
    } else {
        f->eax = 0;
    }
    if (esp3 != 0 && eip3 != 0) {
        current->esp3 = esp3;
        current->eip3 = eip3;
        current->swexn_arg = args[2];
    } else {
        current->eip3 = 0;
    }
    /* clear swexn flag after success swexn call */
    current->df3 = 0;
    return;

bad_user_entry:
    f->eax = (reg_t)-2;
    return;

read_arg_fail:
    f->eax = (reg_t)-1;
    return;
}
