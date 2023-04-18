/** @file kbd.h
 *
 *  @brief keyboard functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _KBD_H_
#define _KBD_H_

#include <common.h>
#include <sched.h>
#include <sync.h>

/**
 * @brief read a line to userspace
 * @param len max length to read
 * @param buf buffer
 * @return size on success, -1 on failure
 */
int do_readline(int len, va_t buf);

/**
 * @brief read a char
 * @return char read
 */
int do_getchar();

#endif
