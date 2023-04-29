/** @file pv.h
 *
 *  @brief PV functions
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _PV_H_
#define _PV_H_

#include <stdlib.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <ureg.h>
#include <x86/eflags.h>
#include <x86/idt.h>
#include <x86/seg.h>

#include <common.h>
#include <loader.h>
#include <paging.h>

/** RPL3 means RPL is 3 */
#define SEGSEL_RPL3 3

/** cs for PV guest */
#define SEGSEL_PV_CS (SEGSEL_SPARE0 | SEGSEL_RPL3)
/** cs index for PV guest */
#define SEGSEL_PV_CS_IDX SEGSEL_SPARE0_IDX
/** ds for PV guest */
#define SEGSEL_PV_DS (SEGSEL_SPARE1 | SEGSEL_RPL3)
/** ds index for PV guest */
#define SEGSEL_PV_DS_IDX SEGSEL_SPARE1_IDX
/** fs for PV guest */
#define SEGSEL_PV_FS (SEGSEL_SPARE3 | SEGSEL_RPL3)
/** fs index for PV guest */
#define SEGSEL_PV_FS_IDX SEGSEL_SPARE3_IDX

/** VM limit for a PV guest */
#define PV_VM_LIMIT (va_t)(~(USER_MEM_START - 1))

/** default memory size (MB) for a PV guest */
#define PV_DEFAULT_SIZE 24
/** minimum memory size (MB) for a PV guest */
#define PV_MINIMUM_SIZE 20

/** shadow page table structure */
typedef struct pv_pd_s {
    int refcount;
    queue_t pv_link; /** in pv_t's shadow_pds */
    pa_t guest_pd;   /** physical address of original guest page table */
    int wp;
    pa_t cr3;      /** shadow page table for PV kernel mode */
    pa_t user_cr3; /** shadow page table for user mode */
} pv_pd_t;

/** DPL_0 means DPL is 0 */
#define VIDT_DPL_0 0
/** DPL_3 means DPL is 3 */
#define VIDT_DPL_3 3
/** all bits used in dpl */
#define VIDT_DPL_MASK 3

/** virtual idt entry structure */
typedef struct pv_idt_entry_s {
    va_t eip;
    int desc; /** currently only contains dpl */
} pv_idt_entry_t;

/** virtual IRQ table */
typedef struct pv_irq_s {
    int pending;
    int arg;
} pv_irq_t;

/** index where fault starts */
#define PV_FAULT_START 0
/** index where fault ends */
#define PV_FAULT_END 20
/** index where IRQ starts */
#define PV_IRQ_START 32
/** index where IRQ ends */
#define PV_IRQ_END 34
/** index where syscall area 1 starts */
#define PV_SYSCALL_1_START 65
/** index where syscall area 1 ends */
#define PV_SYSCALL_1_END 117
/** index where syscall area 2 starts */
#define PV_SYSCALL_2_START 128
/** index where syscall area 2 ends */
#define PV_SYSCALL_2_END 135

/** virtual idt structure */
typedef struct pv_idt_s {
    pv_idt_entry_t fault_irq[PV_IRQ_END - PV_FAULT_START];
    pv_irq_t pending_irq[PV_IRQ_END -
                         PV_IRQ_START]; /** pending keyboard/timer IRQ */
    pv_idt_entry_t syscall_1[PV_SYSCALL_1_END - PV_SYSCALL_1_START];
    pv_idt_entry_t syscall_2[PV_SYSCALL_2_END - PV_SYSCALL_2_START];
} pv_idt_t;

/** PV control block structure */
typedef struct pv_s {
    int n_pages;
    pa_t mem_base;
    int vif;
    pv_pd_t* active_shadow_pd;
    queue_t* shadow_pds;
    pv_idt_t vidt;
    va_t vesp0;
    queue_t pts_link; /** in pts_t's pvs */
} pv_t;

/**
 * @brief initialize PV system
 */
void pv_init();

struct process_s;
typedef struct process_s process_t;
struct thread_s;
typedef struct thread_s thread_t;

/**
 * @brief create a PV guest
 * @param t thread control block
 * @param elf the guest kernel
 * @param exe name of guest kernel file
 * @param mem_size size of guest kernel memory
 * @return the guest kernel thread, NULL on failure
 */
thread_t* create_pv_process(thread_t* t,
                            simple_elf_t* elf,
                            char* exe,
                            va_size_t mem_size);

/**
 * @brief block interrupt for a PV guest
 * @param pv PV control block for PV guest
 */
static inline void pv_mask_interrupt(pv_t* pv) {
    pv->vif = 0;
}

/**
 * @brief unblock interrupt for a PV guest
 * @param pv PV control block for PV guest
 */
static inline void pv_unmask_interrupt(pv_t* pv) {
    pv->vif = EFL_IF;
}

/**
 * @brief find a vidt entry for a interrupt index
 * @param pv PV control block for PV guest
 * @param index interrupt index
 * @return vidt entry, NULL on failure
 */
static inline pv_idt_entry_t* pv_classify_interrupt(pv_t* pv, int index) {
    if (index >= PV_FAULT_START && index < PV_IRQ_END) {
        return &pv->vidt.fault_irq[index - PV_FAULT_START];
    }
    if (index >= PV_SYSCALL_1_START && index < PV_SYSCALL_1_END) {
        return &pv->vidt.syscall_1[index - PV_SYSCALL_1_START];
    }
    if (index >= PV_SYSCALL_2_START && index < PV_SYSCALL_2_END) {
        return &pv->vidt.syscall_2[index - PV_SYSCALL_2_START];
    }
    return NULL;
}

/**
 * @brief destroy a pv control block
 * @param pv PV control block for PV guest
 */
void destroy_pv(pv_t* pv);

/**
 * @brief kill current PV guest and print a reason
 * @param reason reason
 */
void pv_die(char* reason);

/**
 * @brief switch PV guest to kernel/user mode
 * @param p process block of PV guest
 * @param kernelmode is switching to kernel mode?
 */
void pv_switch_mode(process_t* p, int kernelmode);

/**
 * @brief load a shadow page table
 * @param p process block of PV guest
 * @param pv_pd shadow page table
 */
void pv_select_pd(process_t* p, pv_pd_t* pv_pd);

/**
 * @brief handel a fault happened when PV guest is running
 * @param frame fault frame
 * @param t PV guest thread
 */
void pv_handle_fault(ureg_t* frame, thread_t* t);

/**
 * @brief Pend an IRQ request to PV guest
 * @param pv PV control block for PV guest
 * @param index IRQ index
 * @param arg a custom arg to push to PV guest kernel stack
 */
void pv_pend_irq(pv_t* pv, int index, int arg);

struct stack_frame_s;
typedef struct stack_frame_s stack_frame_t;

/**
 * @brief try to inject an IRQ to PV guest, or mark it as pending if the guest
 * is currently blocking interrupt
 * @param f stack frame of PV guest
 * @param index IRQ index
 * @param arg a custom arg to push to PV guest kernel stack
 */
int pv_inject_irq(stack_frame_t* f, int index, int arg);

/**
 * @brief check if there is a pending IRQ and inject it if one exists
 * @param f stack frame of PV guest
 */
void pv_check_pending_irq(stack_frame_t* f);

/**
 * @brief arrange the stack layout for injecting an interrupt (fault/IRQ)
 * @param t PV guest thread
 * @param pv PV control block for PV guest
 * @param f stack frame of PV guest
 * @param arg a custom arg to push to PV guest kernel stack
 * @param eip guest handler address
 */
void pv_inject_interrupt(thread_t* t,
                         pv_t* pv,
                         stack_frame_t* f,
                         int arg,
                         va_t eip);

typedef struct pv_frame_s {
    reg_t cr2;
    reg_t error_code;
    reg_t eip;
    reg_t vcs;
    reg_t eflags;
} pv_frame_t;

#endif
