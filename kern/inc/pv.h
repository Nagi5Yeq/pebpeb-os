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

#define SEGSEL_RPL3 3

#define SEGSEL_PV_CS (SEGSEL_SPARE0 | SEGSEL_RPL3)
#define SEGSEL_PV_CS_IDX SEGSEL_SPARE0_IDX
#define SEGSEL_PV_DS (SEGSEL_SPARE1 | SEGSEL_RPL3)
#define SEGSEL_PV_DS_IDX SEGSEL_SPARE1_IDX
#define SEGSEL_PV_FS (SEGSEL_SPARE3 | SEGSEL_RPL3)
#define SEGSEL_PV_FS_IDX SEGSEL_SPARE3_IDX

#define PV_VM_LIMIT (va_t)(~(USER_MEM_START - 1))

#define PV_DEFAULT_SIZE 24
#define PV_MINIMUM_SIZE 20

typedef struct pv_pd_s {
    int refcount;
    queue_t pv_link;
    pa_t guest_pd;
    int wp;
    pa_t cr3;
    pa_t user_cr3;
} pv_pd_t;

#define VIDT_DPL_0 0
#define VIDT_DPL_3 3
#define VIDT_DPL_MASK 3

typedef struct pv_idt_entry_s {
    va_t eip;
    int desc;
} pv_idt_entry_t;

typedef struct pv_irq_s {
    int pending;
    int arg;
} pv_irq_t;

#define PV_FAULT_START 0
#define PV_FAULT_END 20
#define PV_IRQ_START 32
#define PV_IRQ_END 34
#define PV_SYSCALL_1_START 65
#define PV_SYSCALL_1_END 117
#define PV_SYSCALL_2_START 128
#define PV_SYSCALL_2_END 135

typedef struct pv_idt_s {
    pv_idt_entry_t fault[PV_FAULT_END - PV_FAULT_START];
    pv_idt_entry_t irq[PV_IRQ_END - PV_IRQ_START];
    pv_irq_t pending_irq[PV_IRQ_END - PV_IRQ_START];
    pv_idt_entry_t syscall_1[PV_SYSCALL_1_END - PV_SYSCALL_1_START];
    pv_idt_entry_t syscall_2[PV_SYSCALL_2_END - PV_SYSCALL_2_START];
} pv_idt_t;

typedef struct pv_s {
    int n_pages;
    pa_t mem_base;
    int vif;
    pv_pd_t* active_shadow_pd;
    queue_t* shadow_pds;
    pv_idt_t vidt;
    va_t vesp0;
} pv_t;

void pv_init();

struct process_s;
typedef struct process_s process_t;
struct thread_s;
typedef struct thread_s thread_t;

thread_t* create_pv_process(thread_t* t,
                            simple_elf_t* elf,
                            char* exe,
                            va_size_t mem_size);

static inline void pv_mask_interrupt(pv_t* pv) {
    pv->vif = 0;
}

static inline void pv_unmask_interrupt(pv_t* pv) {
    pv->vif = EFL_IF;
}

static inline pv_idt_entry_t* pv_classify_interrupt(pv_t* pv, int index) {
    if (index >= PV_FAULT_START && index < PV_FAULT_END) {
        return &pv->vidt.fault[index - PV_FAULT_START];
    }
    if (index >= PV_IRQ_START && index < PV_IRQ_END) {
        return &pv->vidt.irq[index - PV_IRQ_START];
    }
    if (index >= PV_SYSCALL_1_START && index < PV_SYSCALL_1_END) {
        return &pv->vidt.syscall_1[index - PV_SYSCALL_1_START];
    }
    if (index >= PV_SYSCALL_2_START && index < PV_SYSCALL_2_END) {
        return &pv->vidt.syscall_2[index - PV_SYSCALL_2_START];
    }
    return NULL;
}

void destroy_pv(pv_t* pv);

void pv_die(char* reason);

void pv_switch_mode(process_t* p, int kernelmode);

void pv_select_pd(process_t* p, pv_pd_t* pv_pd);

void pv_handle_fault(ureg_t* frame, thread_t* t);

struct stack_frame_s;
typedef struct stack_frame_s stack_frame_t;
int pv_inject_irq(stack_frame_t* f, int index, int arg);

void pv_check_pending_irq(stack_frame_t* f);

void do_inject_irq(thread_t* t, pv_t* pv, stack_frame_t* f, int arg, va_t eip);

typedef struct pv_frame_s {
    reg_t cr2;
    reg_t error_code;
    reg_t eip;
    reg_t vcs;
    reg_t eflags;
} pv_frame_t;

#endif
