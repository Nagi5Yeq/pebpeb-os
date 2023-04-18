/** @file timer.h
 *
 *  @brief timer functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/** ticks passed */
extern unsigned int ticks;
/** timer heap lock */
extern spl_t timer_lock;
/** timers of sleeping threads */
extern heap_t timers;

/**
 * @brief initialize the timer
 */
void timer_init();

#endif
