/** @file sched.h
 *
 *  @brief scheduling functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _SCHED_H_
#define _SCHED_H_

#include <common_kern.h>
#include <elf/elf_410.h>

#include <x86/eflags.h>
#include <x86/seg.h>

#include <common.h>
#include <paging.h>
#include <pv.h>
#include <sync.h>

/** name of idle process */
#define IDLE_NAME "idle"
/** name of init process */
#define INIT_NAME "init"

#define IDLE_PID 2
#define INIT_PID 1

/** we use fs to find percpu area */
#define SEGSEL_KERNEL_FS SEGSEL_SPARE2
/** index of fs in gdt */
#define SEGSEL_KERNEL_FS_IDX SEGSEL_SPARE2_IDX

/* G means global segment */
#define GDT_G_BIT 0x0080000000000000ull
/* FLAG is control flags for segment */
#define GDT_FLAG_MASK 0x00f0ff0000000000ull
/* high part of base address */
#define GDT_BASE_MASK_HI 0xff000000ull
/* low part of base address */
#define GDT_BASE_MASK_LO 0x00ffffffull
/* position of high part of base address */
#define GDT_BASE_SHIFT_HI (64 - 32)
/* position of low part of base address */
#define GDT_BASE_SHIFT_LO (40 - 24)
/* high part of limit */
#define GDT_LIMIT_MASK_HI 0xf0000ull
/* low part of limit */
#define GDT_LIMIT_MASK_LO 0x0ffffull
/* position of high part of limit */
#define GDT_LIMIT_SHIFT_HI (52 - 20)
/* position of low part of limit */
#define GDT_LIMIT_SHIFT_LO (16 - 16)

/** an allocated memory of a process */
typedef struct region_s {
    va_t addr;
    va_size_t size; /* size in bytes */
    pa_t paddr;
    int is_rw;
} region_t;

/**
 * default size for kernel thread, as long as kernel does not contain recursion
 * and do not allocate big local variable, the kernel stack pointer will never
 * overflow
 */
#define K_STACK_SIZE PAGE_SIZE

/** initial eflags, interrupt enabled and IOPL=0 */
#define DEFAULT_EFLAGS (EFL_IF | EFL_IOPL_RING0 | EFL_RESV1)

/** frame used for kernel context switching */
typedef struct yield_frame_s {
    reg_t ebp;
    reg_t ebx;
    reg_t eflags;
    reg_t raddr;
} yield_frame_t;

/** frame used for syscall and interrupts (not faults) */
typedef struct stack_frame_s {
    reg_t gs;
    reg_t fs;
    reg_t es;
    reg_t ds;
    reg_t edi;
    reg_t esi;
    reg_t ebp;
    reg_t dummy_esp; /* gap of pusha */
    reg_t ebx;
    reg_t edx;
    reg_t ecx;
    reg_t eax;
    reg_t eip;
    reg_t cs;
    reg_t eflags;
    reg_t esp; /* not present if interrupted in kernel mode */
    reg_t ss;  /* not present if interrupted in kernel mode */
} stack_frame_t;

/** process control block */
typedef struct process_s {
    int pid;
    int exit_value;
    struct process_s* parent;
    queue_t sible_link; /* in parent process' child queue */

    mutex_t refcount_lock;
    int refcount;
    queue_t* threads;

    int nchilds; /* live and dead but unclaimed childs */
    queue_t* live_childs;
    queue_t* dead_childs;

    int nwaiters; /* number of threads calling wait() */
    mutex_t wait_lock;
    cv_t wait_cv;

    pa_t cr3;
    vector_t regions; /* record the vitural memories the user has mapped */
    mutex_t mm_lock;  /* lock when operating VM */

    pv_t* pv;
} process_t;

/** status of the thread, must lock ready_lock to change it, otherwise may be
 * changed back to READY by timer interrupt */
typedef enum thr_stat_e {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED, /* blocked on a mutex/cv */
    THREAD_SLEEPING,
    THREAD_DESCHEDULED,
    THREAD_DEAD
} thr_stat_t;

/** thread control block */
typedef struct thread_s {
    rb_t rb_node; /* rbtree node, key is tid */
    thr_stat_t status;
    spl_t status_lock;
    queue_t sched_link; /* in ready queue or other queue */
    int pending_exit;   /* if a task_vanish is pending */

    queue_t process_link; /* in process_t's threads queue */

    process_t* process;

    reg_t esp3;
    reg_t eip3;
    reg_t swexn_arg;
    int df3; /* is a fault is currently being handled by swexn handler */

    reg_t esp0; /* esp to use when switching from ring3 to ring0
                   (fault/interrupt) */
    reg_t eip0; /* handler for faults happen in ring0 */

    reg_t kernel_esp; /* saved kernel esp when swapped out */
    char* stack;      /* kernel stack */
} thread_t;

/**
 * @brief get next available tid
 * @return tid
 */
int alloc_tid();

/** initial stack size for user program, 16 pages is a middle size that do not
 * waste too much and big enough for arguments  */
#define DEFAULT_STACK_SIZE (65536)
/** initial stack end for user program, skip the last page as it would cause
 * round bugs */
#define DEFAULT_STACK_END (0xffffe000)
/** inital stack area start */
#define DEFAULT_STACK_POS (DEFAULT_STACK_END - DEFAULT_STACK_SIZE)

/** maximum arg length for exec(), a middle size */
#define MAX_ARG_LEN 4096
/** maximum number of args for exec(), a middle size */
#define MAX_NUM_ARG 256
/** maximum total arg length for exec(), save last page for argc, argv, stack_hi
 * and stack_lo */
#define MAX_TOTAL_ARG_LEN (DEFAULT_STACK_SIZE - PAGE_SIZE)

uint64_t create_segsel(va_t base, va_size_t limit, uint64_t flags);

/**
 * @brief create an empty process with one thread and no user memory
 * @return the thread created or NULL on failure
 */
thread_t* create_empty_process();

/**
 * @brief create a process and load the executable and push the arguments
 * @param tid tid to use, or 0 if we want to allocate a new one
 * @param exec name of executable to load
 * @param argc number of arguments
 * @param argv array of arguments
 * @return the thread created or NULL on failure
 */
thread_t* create_process(int tid, char* exe, int argc, const char** argv);

/**
 * @brief destroy a thread which is never run
 * @param t the thread to destroy
 */
void destroy_thread(thread_t* t);

void destroy_pd(pa_t pd_pa);

/**
 * @brief add a memory region to a process
 * @param p the process
 * @param addr virtual address
 * @param n_pages number of pages in this region
 * @param pa physical address
 * @param is_rw is this region writeable
 * @return 0 for success, -1 for failure
 */
int add_region(process_t* p, va_t vaddr, int n_pages, pa_t pa, int is_rw);

/**
 * @brief find the page table for addr, or allocate one if not present
 * @param p the process
 * @param addr virtual address
 * @return physical address of page table or BAD_PA on failure
 */
pa_t find_or_create_pt(process_t* p, va_t vaddr);

/** CPU specific varibles */
typedef struct percpu_s {
    thread_t* current;           /* current running thread */
    thread_t* idle;              /* idle thread */
    thread_t* kthread;           /* original kernel thread */
    va_t mapped_phys_page;       /* physical page mapping area */
    pte_t* mapped_phys_page_pte; /* pte for the mapping area */
} percpu_t;

/**
 * @brief initialize fs segment's base to percpu structure
 * @param percpu address of percpu structure
 */
void setup_percpu(percpu_t* percpu);

/**
 * @brief setup the kernel thread's control structure, which needed when
 * switching to othre processes when startup
 * @param kthread kernel thread's thread control structure
 * @param kprocess  kernel thread's process control structure
 */
void setup_kth(thread_t* kthread, process_t* kprocess);

/**
 * @brief get current running thread
 * @return current running thread
 */
thread_t* get_current();
/**
 * @brief set current running thread
 * @param t current running thread
 */
void set_current(thread_t* t);
/**
 * @brief get idle thread
 * @return idle thread
 */
thread_t* get_idle();
/**
 * @brief set idle thread
 * @param t idle thread
 */
void set_idle(thread_t* t);
/**
 * @brief get kernel thread
 * @return kernel thread
 */
thread_t* get_kthread();
/**
 * @brief set kernel thread
 * @param t kernel thread
 */
void set_kthread(thread_t* t);
/**
 * @brief get physical page mapping area
 * @return physical page mapping area
 */
va_t get_mapped_phys_page();
/**
 * @brief set physical page mapping area
 * @param va physical page mapping area
 */
void set_mapped_phys_page(va_t va);
/**
 * @brief get pte for physical page mapping area
 * @return pte for physical page mapping area
 */
pte_t* get_mapped_phys_page_pte();
/**
 * @brief set pte for physical page mapping area
 * @param pte pte for physical page mapping area
 */
void set_mapped_phys_page_pte(pte_t* pte);

/**
 * @brief swap current thread's process and newt's process, newt must be a new
 * thread with new process, current thread must be the only thread in a process
 * @param newt thread to swap
 */
void swap_process_inplace(thread_t* newt);

/** init process */
extern process_t* init_process;

/** lock for ready queue */
extern spl_t ready_lock;
/** queue of ready threads */
extern queue_t* ready;

/** rbtree of all threads */
extern rb_t* threads;
/** mutex for rbtree */
extern mutex_t threads_lock;

/**
 * @brief insert a thread to the tail of ready queue, must lock ready_lock
 * before calling this function
 * @param t thread to insert
 */
void insert_ready_tail(thread_t* t);

/**
 * @brief insert a thread to the head of ready queue, must lock ready_lock
 * before calling this function
 * @param t thread to insert
 */
void insert_ready_head(thread_t* t);

/**
 * @brief select next ready thread
 * @return the thread
 */
thread_t* select_next();

/**
 * @brief save current esp, load t's cr3, esp0 and return t's esp, must disable
 * interrupt before calling
 * @param t thread to load
 * @param esp current esp
 * @return new esp
 */
reg_t save_and_setup_env(thread_t* t, reg_t esp);

/**
 * @brief let current thread vanish
 */
void kill_current();

/**
 * @brief switch to another stack and cleanup t's thread control block and stack
 */
void switch_stack_cleanup(reg_t esp, thread_t* t);

/**
 * @brief check pending exit request and pop saved user registers and return to
 * ring 3
 */
void return_to_user();

/**
 * @brief find a thread from rbtree, must lock threads_lock before calling
 * @param tid tid of thread
 * @return the thread or NULL if not found
 */
thread_t* find_thread(int tid);

/**
 * @brief add a thread to rbtree, will automatically lock threads_lock
 * @param t thread to add
 */
void add_thread(thread_t* t);

/**
 * @brief remove a thread from rbtree, will automatically lock threads_lock
 * @param t thread to remove
 */
void remove_thread(thread_t* t);

#endif
