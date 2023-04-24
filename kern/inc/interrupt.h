/** @file interrupt.h
 *
 *  @brief idt initializer.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <x86/seg.h>

#include <paging.h>

/**
 * IRQ # of timer
 */
#define TIMER_IRQ 0

/**
 * IRQ # of keyboard
 */
#define KBD_IRQ 1

/**
 * @brief an idt entry
 */
typedef struct idt_s {
    unsigned long lo;
    unsigned long hi;
} idt_t;

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

#define IDT_FAULT_15 15

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
 * @brief initialize idt
 */
void idt_init();

#endif
