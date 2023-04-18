/**
 * @file dog/inc/devices.h
 * @brief Header for special-purpose "device drivers"
 * @author jrduvall
 * @author de0u
 *
 */

#ifndef DOG_DEVICES_H
#define DOG_DEVICES_H

/**
 * @brief Gets the next augchar via virtual interrupt handling
 */
int augchar(void);

/**
 * @brief Handles virtual keyboard interrupts
 */
void kbd_intr(void);

/**
 * @brief Handles virtual timer interrupts
 */
void timer_intr(void);

/**
 * @brief Return count of ticks since boot
 */
int tick_count(void);

#endif /* DOG_DEVICES_H */
