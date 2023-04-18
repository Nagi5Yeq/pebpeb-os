/** @file timer.c
 *
 *  @brief timer functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <common_kern.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <x86/asm.h>
#include <x86/interrupt_defines.h>
#include <x86/timer_defines.h>

#include <interrupt.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>

unsigned int ticks = 0;

spl_t timer_lock = SPL_INIT;

heap_t timers;

/* we want a 2ms thread switch but since sometimes interrupts are disabled, we
 * set a higher rate to compensate this
 */
#define TIMER_FREQ 1000

void timer_init() {
    int counter = TIMER_RATE / TIMER_FREQ;
    outb(TIMER_MODE_IO_PORT, TIMER_SQUARE_WAVE);
    outb(TIMER_PERIOD_IO_PORT, counter & 0xFF);
    outb(TIMER_PERIOD_IO_PORT, (counter >> 8) & 0xFF);
    if (heap_init(&timers) != 0) {
        panic("no space to initialize timer heap");
    }
}

/**
 * @brief check expired timers
 */
static void check_timers() {
    int old_if = spl_lock(&timer_lock);
    heap_node_t* node;
    while ((node = heap_peak(&timers)) != NULL) {
        if (node->key > ticks) {
            break;
        }
        thread_t* t = (thread_t*)node->value;
        heap_pop(&timers);
        int old_if2 = spl_lock(&ready_lock);
        insert_ready_tail(t);
        spl_unlock(&ready_lock, old_if2);
    }
    spl_unlock(&timer_lock, old_if);
}

void timer_handler_real() {
    pic_acknowledge(TIMER_IRQ);
    ticks++;
    check_timers();
    int old_if = spl_lock(&ready_lock);
    thread_t* current = get_current();
    if (current != get_idle()) {
        insert_ready_tail(current);
    }
    thread_t* t = select_next();
    yield_to_spl_unlock(t, &ready_lock, old_if);
}
