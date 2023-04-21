#ifndef _PV_H_
#define _PV_H_

#include <common_kern.h>
#include <elf/elf_410.h>
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

#define PV_VM_LIMIT (~(USER_MEM_START - 1))

#define PV_DEFAULT_SIZE 24
#define PV_MINIMUM_SIZE 20

typedef struct pv_pd_s {
    int refcount;
    queue_t pv_link;
    pa_t cr3;
    pa_t user_cr3;
} pv_pd_t;

typedef struct pv_idt_entry_s {
    va_t eip;
    int dpl;
} pv_idt_entry_t;

typedef pv_idt_entry_t pv_idt_t[IDT_ENTS];

typedef struct pv_s {
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

void destroy_pv(pv_t* pv);

#endif
