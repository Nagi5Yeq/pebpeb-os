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

#include <apic.h>
#include <assert.h>
#include <x86/asm.h>
#include <x86/idt.h>
#include <x86/interrupt_defines.h>
#include <x86/timer_defines.h>

#include <interrupt.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>

unsigned int ticks = 0;

spl_t timer_lock = SPL_INIT;

heap_t timers;

static uint32_t lapic_dt;

/* 2ms */
#define TIMER_FREQ 500

int timer_count;

void timer_test_handler();

void timer_init() {
    /* use 10x slower frequency for APIC timer testing */
    int counter = TIMER_RATE / (TIMER_FREQ / 10);
    outb(TIMER_MODE_IO_PORT, TIMER_ONE_SHOT);
    outb(TIMER_PERIOD_IO_PORT, counter & 0xFF);
    outb(TIMER_PERIOD_IO_PORT, (counter >> 8) & 0xFF);
    if (heap_init(&timers) != 0) {
        panic("no space to initialize timer heap");
    }
    idt_t* idt = (idt_t*)idt_base();
    idt_t old_idt = idt[TIMER_IDT_ENTRY];
    idt[TIMER_IDT_ENTRY] =
        make_idt((va_t)timer_test_handler, IDT_TYPE_I32, IDT_DPL_KERNEL);

    lapic_write(LAPIC_LVT_TIMER, (LAPIC_ONESHOT | TIMER_IDT_ENTRY));
    lapic_write(LAPIC_TIMER_DIV, LAPIC_X1);
    lapic_write(LAPIC_TIMER_INIT, 0xffffffff);

    timer_count = 10;
    while (1) {
        int old_timer_count = timer_count;
        outb(TIMER_MODE_IO_PORT, TIMER_ONE_SHOT);
        outb(TIMER_PERIOD_IO_PORT, counter & 0xFF);
        outb(TIMER_PERIOD_IO_PORT, (counter >> 8) & 0xFF);
        enable_interrupts();
        while (timer_count == old_timer_count) {
        }
        disable_interrupts();
        if (timer_count == 0) {
            break;
        }
    }

    lapic_dt = (0xffffffff - lapic_read(LAPIC_TIMER_CUR)) / 100;
    lapic_write(LAPIC_TIMER_INIT, 0);
    idt[TIMER_IDT_ENTRY] = old_idt;
}

void setup_lapic_timer() {
    lapic_write(LAPIC_LVT_TIMER, (LAPIC_PERIODIC | TIMER_IDT_ENTRY));
    lapic_write(LAPIC_TIMER_DIV, LAPIC_X1);
    lapic_write(LAPIC_TIMER_INIT, lapic_dt);
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

void timer_handler_real(stack_frame_t* f) {
    apic_eoi();
    ticks++;
    check_timers();
    pv_inject_irq(f, TIMER_IDT_ENTRY, 0);
    int old_if = spl_lock(&ready_lock);
    thread_t* current = get_current();
    if (current != get_idle()) {
        insert_ready_tail(current);
    }
    thread_t* t = select_next();
    yield_to_spl_unlock(t, &ready_lock, old_if);
}
