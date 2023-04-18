/**
 * @file panic.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief panic implementation
 * @date 2023-02-22
 *
 */

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief print msg and die
 */
void panic(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);

    vprintf(fmt, va);

    va_end(va);
    
    exit(-1);
}
