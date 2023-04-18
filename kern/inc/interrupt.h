/** @file interrupt.h
 *
 *  @brief idt initializer.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

/**
 * IRQ # of timer
 */
#define TIMER_IRQ 0

/**
 * IRQ # of keyboard
 */
#define KBD_IRQ 1

/**
 * @brief initialize idt
 */
void idt_init();

#endif
