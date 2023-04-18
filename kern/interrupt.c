/** @file interrupt.c
 *
 *  @brief idt initializer and fault/interrupt handlers.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <x86/asm.h>
#include <x86/eflags.h>
#include <x86/idt.h>
#include <x86/interrupt_defines.h>
#include <x86/seg.h>
#include <x86/timer_defines.h>

#include <keyhelp.h>
#include <simics.h>
#include <stdio.h>
#include <string.h>
#include <syscall_int.h>
#include <ureg.h>

#include <asm_instr.h>
#include <interrupt.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>
#include <usermem.h>

/** means this entry requires ring 0 privilige */
#define IDT_DPL_KERNEL 0
/** means this entry requires ring 3 privilige */
#define IDT_DPL_USER 3
/** means this entry is valid */
#define IDT_P 1
/** means this entry is a 32 bit interrupt */
#define IDT_TYPE_I32 0xe
/** means this entry is a 32 bit trap */
#define IDT_TYPE_T32 0xf

/** P bit's position */
#define IDT_P_SHIFT (47 - 32)
/** DPL's position */
#define IDT_DPL_SHIFT (45 - 32)
/** TYPE's position */
#define IDT_TYPE_SHIFT (40 - 32)
/** CS's position */
#define IDT_CS_SHIFT 16

/** EIP's high part's bits */
#define IDT_EIP_HI_MASK 0xffff0000
/** EIP's low part's bits */
#define IDT_EIP_LO_MASK 0x0000ffff

/**
 * @brief an idt entry
 */
typedef struct idt_s {
    unsigned long lo;
    unsigned long hi;
} idt_t;

/**
 * @brief create an idt entry
 * @param eip eip
 * @param type type
 * @param dpl dpl
 * @return the idt entry
 */
static inline idt_t make_idt(va_t eip, int type, int dpl) {
    idt_t idt;
    idt.hi = ((eip & IDT_EIP_HI_MASK) | (IDT_P << IDT_P_SHIFT) |
              (dpl << IDT_DPL_SHIFT) | (type << IDT_TYPE_SHIFT));
    idt.lo = ((SEGSEL_KERNEL_CS << IDT_CS_SHIFT) | (eip & IDT_EIP_LO_MASK));
    return idt;
}

/**
 * @brief DE handler entry
 */
int de_handler();
/**
 * @brief DB handler entry
 */
int db_handler();
/**
 * @brief NMI handler entry
 */
int nmi_handler();
/**
 * @brief BP handler entry
 */
int bp_handler();
/**
 * @brief OF handler entry
 */
int of_handler();
/**
 * @brief BR handler entry
 */
int br_handler();
/**
 * @brief UD handler entry
 */
int ud_handler();
/**
 * @brief NM handler entry
 */
int nm_handler();
/**
 * @brief DF handler entry
 */
int df_handler();
/**
 * @brief CSO handler entry
 */
int cso_handler();
/**
 * @brief TS handler entry
 */
int ts_handler();
/**
 * @brief NP handler entry
 */
int np_handler();
/**
 * @brief SS handler entry
 */
int ss_handler();
/**
 * @brief GP handler entry
 */
int gp_handler();
/**
 * @brief PF handler entry
 */
int pf_handler();
/**
 * @brief MF handler entry
 */
int mf_handler();
/**
 * @brief AC handler entry
 */
int ac_handler();
/**
 * @brief MC handler entry
 */
int mc_handler();
/**
 * @brief XF handler entry
 */
int xf_handler();

/**
 * @brief default handler entry
 */
int default_handler();

/**
 * @brief timer handler entry
 */
void timer_handler();
/**
 * @brief keyboard handler entry
 */
void kbd_handler();

/**
 * @brief fork() syscall entry
 */
void sys_fork();
/**
 * @brief exec() syscall entry
 */
void sys_exec();
/**
 * @brief wait() syscall entry
 */
void sys_wait();
/**
 * @brief yield() syscall entry
 */
void sys_yield();
/**
 * @brief deschedule() syscall entry
 */
void sys_deschedule();
/**
 * @brief make_runnable() syscall entry
 */
void sys_make_runnable();
/**
 * @brief gettid() syscall entry
 */
void sys_gettid();
/**
 * @brief new_pages() syscall entry
 */
void sys_new_pages();
/**
 * @brief remove_pages() syscall entry
 */
void sys_remove_pages();
/**
 * @brief sleep() syscall entry
 */
void sys_sleep();
/**
 * @brief getchar() syscall entry
 */
void sys_getchar();
/**
 * @brief readline() syscall entry
 */
void sys_readline();
/**
 * @brief print() syscall entry
 */
void sys_print();
/**
 * @brief set_term_color() syscall entry
 */
void sys_set_term_color();
/**
 * @brief set_cursor_pos() syscall entry
 */
void sys_set_cursor_pos();
/**
 * @brief get_cursor() syscall entry
 */
void sys_get_cursor_pos();
/**
 * @brief thread() syscall entry
 */
void sys_thread_fork();
/**
 * @brief get_ticks() syscall entry
 */
void sys_get_ticks();
/**
 * @brief misbehave() syscall entry
 */
void sys_misbehave();
/**
 * @brief halt() syscall entry
 */
void sys_halt();
/**
 * @brief task_vanish() syscall entry
 */
void sys_task_vanish();
/**
 * @brief set_status() syscall entry
 */
void sys_set_status();
/**
 * @brief vanish() syscall entry
 */
void sys_vanish();
/**
 * @brief readfile() syscall entry
 */
void sys_readfile();
/**
 * @brief swexn() syscall entry
 */
void sys_swexn();

/**
 * @brief all other syscall entries
 */
void sys_enosys();

/** index of first syscall */
#define IDT_SYSCALL_START (X86_PIC_MASTER_IRQ_BASE + 16)

void idt_init() {
    idt_t* idt = (idt_t*)idt_base();

    int i;
    for (i = 0; i < IDT_SYSCALL_START; i++) {
        idt[i] = make_idt((va_t)default_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    }

    idt[IDT_DE] = make_idt((va_t)de_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_DB] = make_idt((va_t)db_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_NMI] = make_idt((va_t)nmi_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_BP] = make_idt((va_t)bp_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_OF] = make_idt((va_t)of_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_BR] = make_idt((va_t)br_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_UD] = make_idt((va_t)ud_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_NM] = make_idt((va_t)nm_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_DF] = make_idt((va_t)df_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_CSO] = make_idt((va_t)cso_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_TS] = make_idt((va_t)ts_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_NP] = make_idt((va_t)np_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_SS] = make_idt((va_t)ss_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_GP] = make_idt((va_t)gp_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_PF] = make_idt((va_t)pf_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_MF] = make_idt((va_t)mf_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_AC] = make_idt((va_t)ac_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_MC] = make_idt((va_t)mc_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[IDT_XF] = make_idt((va_t)xf_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);

    idt[TIMER_IDT_ENTRY] =
        make_idt((va_t)timer_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);
    idt[KEY_IDT_ENTRY] =
        make_idt((va_t)kbd_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);

    for (i = IDT_SYSCALL_START; i < IDT_ENTS; i++) {
        idt[i] = make_idt((va_t)sys_enosys, IDT_TYPE_T32, IDT_DPL_USER);
    }

    idt[FORK_INT] = make_idt((va_t)sys_fork, IDT_TYPE_T32, IDT_DPL_USER);
    idt[EXEC_INT] = make_idt((va_t)sys_exec, IDT_TYPE_T32, IDT_DPL_USER);
    idt[WAIT_INT] = make_idt((va_t)sys_wait, IDT_TYPE_T32, IDT_DPL_USER);
    idt[YIELD_INT] = make_idt((va_t)sys_yield, IDT_TYPE_T32, IDT_DPL_USER);
    idt[DESCHEDULE_INT] =
        make_idt((va_t)sys_deschedule, IDT_TYPE_T32, IDT_DPL_USER);
    idt[MAKE_RUNNABLE_INT] =
        make_idt((va_t)sys_make_runnable, IDT_TYPE_T32, IDT_DPL_USER);
    idt[GETTID_INT] = make_idt((va_t)sys_gettid, IDT_TYPE_T32, IDT_DPL_USER);
    idt[NEW_PAGES_INT] =
        make_idt((va_t)sys_new_pages, IDT_TYPE_T32, IDT_DPL_USER);
    idt[REMOVE_PAGES_INT] =
        make_idt((va_t)sys_remove_pages, IDT_TYPE_T32, IDT_DPL_USER);
    idt[SLEEP_INT] = make_idt((va_t)sys_sleep, IDT_TYPE_T32, IDT_DPL_USER);
    idt[GETCHAR_INT] = make_idt((va_t)sys_getchar, IDT_TYPE_T32, IDT_DPL_USER);
    idt[READLINE_INT] =
        make_idt((va_t)sys_readline, IDT_TYPE_T32, IDT_DPL_USER);
    idt[PRINT_INT] = make_idt((va_t)sys_print, IDT_TYPE_T32, IDT_DPL_USER);
    idt[SET_TERM_COLOR_INT] =
        make_idt((va_t)sys_set_term_color, IDT_TYPE_T32, IDT_DPL_USER);
    idt[SET_CURSOR_POS_INT] =
        make_idt((va_t)sys_set_cursor_pos, IDT_TYPE_T32, IDT_DPL_USER);
    idt[GET_CURSOR_POS_INT] =
        make_idt((va_t)sys_get_cursor_pos, IDT_TYPE_T32, IDT_DPL_USER);
    idt[THREAD_FORK_INT] =
        make_idt((va_t)sys_thread_fork, IDT_TYPE_T32, IDT_DPL_USER);
    idt[GET_TICKS_INT] =
        make_idt((va_t)sys_get_ticks, IDT_TYPE_T32, IDT_DPL_USER);
    idt[MISBEHAVE_INT] =
        make_idt((va_t)sys_misbehave, IDT_TYPE_T32, IDT_DPL_USER);
    idt[HALT_INT] = make_idt((va_t)sys_halt, IDT_TYPE_T32, IDT_DPL_USER);
    idt[TASK_VANISH_INT] =
        make_idt((va_t)sys_task_vanish, IDT_TYPE_T32, IDT_DPL_USER);
    idt[SET_STATUS_INT] =
        make_idt((va_t)sys_set_status, IDT_TYPE_T32, IDT_DPL_USER);
    idt[VANISH_INT] = make_idt((va_t)sys_vanish, IDT_TYPE_T32, IDT_DPL_USER);
    idt[READFILE_INT] =
        make_idt((va_t)sys_readfile, IDT_TYPE_T32, IDT_DPL_USER);
    idt[SWEXN_INT] = make_idt((va_t)sys_swexn, IDT_TYPE_T32, IDT_DPL_USER);
}

/** string explanation of fault */
const static char* reasons[] = {"Division Error",
                                "Debug",
                                "Non-maskable Interrupt",
                                "Breakpoint",
                                "Overflow",
                                "Bound Range Exceeded",
                                "Invalid Opcode",
                                "Device Not Available",
                                "Double Fault",
                                "Coprocessor Segment Overrun",
                                "Invalid TSS",
                                "Segment Not Present",
                                "Stack-Segment Fault",
                                "General Protection Fault",
                                "Page Fault",
                                "Reserved",
                                "x87 Floating-Point Exception",
                                "Alignment Check",
                                "Machine Check",
                                "SIMD Floating-Point Exception",
                                "Virtualization Exception",
                                "Control Protection Exception",
                                "Reserved",
                                "Reserved",
                                "Reserved",
                                "Reserved",
                                "Reserved",
                                "Reserved",
                                "Hypervisor Injection Exception",
                                "VMM Communication Exception",
                                "Security Exception",
                                "Reserved"};

/**
 * @brief dump register information
 * @param frame ureg registers
 */
static void dump_fault(ureg_t* frame);

/**
 * @brief handle a fault
 * @param frame ureg registers
 */
void handle_fault(ureg_t* frame) {
    thread_t* current = get_current();
    if (frame->cause == SWEXN_CAUSE_PAGEFAULT && frame->cr2 >= USER_MEM_START) {
        /* check ZFOD */
        pa_t old_pa;
        page_directory_t* pd =
            (page_directory_t*)map_phys_page(current->process->cr3, &old_pa);
        pde_t* pde = &(*pd)[get_pd_index(frame->cr2)];
        if (*pde != BAD_PDE) {
            pa_t pt_pa = get_page_table(*pde);
            page_table_t* pt = (page_table_t*)map_phys_page(pt_pa, NULL);
            pte_t* pte = &(*pt)[get_pt_index(frame->cr2)];
            if (*pte != BAD_PTE &&
                ((*pte & (PTE_PRESENT << PTE_P_SHIFT)) == 0)) {
                *pte = (*pte | (PTE_PRESENT << PTE_P_SHIFT));
                invlpg(frame->cr2);
                memset((void*)(frame->cr2 & PAGE_BASE_MASK), 0, PAGE_SIZE);
                map_phys_page(old_pa, NULL);
                return;
            }
        }
        map_phys_page(old_pa, NULL);
    }
    if (frame->cs == SEGSEL_KERNEL_CS) {
        /* recover from accessing user memory */
        if ((frame->cause == SWEXN_CAUSE_PAGEFAULT ||
             frame->cause == SWEXN_CAUSE_PROTFAULT) &&
            current->eip0 != 0) {
            frame->eip = current->eip0;
            return;
        }
        /* all other non recoverable fault */
        dump_fault(frame);
        while (1) {
        }
    }
    /* send to user swexn */
    if (current->eip3 != 0 && current->df3 == 0) {
        va_t esp3 = (va_t)current->esp3;
        esp3 -= sizeof(ureg_t);
        esp3 = (esp3 & (~(sizeof(va_t) - 1)));
        /* push ureg */
        if (copy_to_user(esp3, sizeof(ureg_t), frame) != 0) {
            goto kill_thread;
        }
        /* push swexn args (return address, swexn arg, ureg)
         * swexn handler must call swexn to jump back, we use 0 address to cause
         * a GP on function return
         */
        unsigned long stack[3] = {0, current->swexn_arg, (unsigned long)esp3};
        unsigned long* new_esp = (unsigned long*)esp3;
        new_esp -= 3;
        if (copy_to_user((va_t)new_esp, 3 * sizeof(unsigned long), stack) !=
            0) {
            goto kill_thread;
        }
        frame->edi = 0;
        frame->esi = 0;
        frame->ebp = 0;
        frame->zero = 0;
        frame->ebx = 0;
        frame->edx = 0;
        frame->ecx = 0;
        frame->eax = 0;
        frame->eip = current->eip3;
        frame->eflags = DEFAULT_EFLAGS;
        frame->esp = (unsigned int)new_esp;
        current->df3 = 1;
        current->eip3 = 0;
        return;
    }
    /* no swexn handler or fault inside swexn */
kill_thread:
    sim_printf("LWP %d killed: %s", current->rb_node.key,
               reasons[frame->cause]);
    printf("LWP %d killed: %s\n", current->rb_node.key, reasons[frame->cause]);
    if (current->process->refcount == 1) {
        current->process->exit_value = -2;
    }
    kill_current();
}

static void dump_fault(ureg_t* frame) {
    sim_printf("Fault: %s  Error Code: %08x", reasons[frame->cause],
               frame->error_code);
    sim_printf("CS:EIP=%04x:%08x  EFLAGS=%08x", frame->cs, frame->eip,
               frame->eflags);
    sim_printf("EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x", frame->eax, frame->ebx,
               frame->ecx, frame->edx);
    sim_printf("ESI=%08x  EDI=%08x  ESP=%08x  EBP=%08x", frame->esi, frame->edi,
               frame->esp, frame->ebp);
    printf("=========================\n");
    printf("Fault: %s  Error Code: %08x\n", reasons[frame->cause],
           frame->error_code);
    printf("CS:EIP=%04x:%08x  EFLAGS=%08x\n", frame->cs, frame->eip,
           frame->eflags);
    printf("EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n", frame->eax, frame->ebx,
           frame->ecx, frame->edx);
    printf("ESI=%08x  EDI=%08x  ESP=%08x  EBP=%08x\n", frame->esi, frame->edi,
           frame->esp, frame->ebp);
}