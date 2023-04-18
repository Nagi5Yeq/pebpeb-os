/** @file usermem.h
 *
 *  @brief userspace r/w functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _USERMEM_H_
#define _USERMEM_H_

#include <stdint.h>

#include <common_kern.h>

#include <paging.h>

/**
 * @brief copy bytes from userspace
 * @param addr address of user memory
 * @param size number of bytes
 * @param buf buffer
 * @return 0 on success, -1 on failure
 */
int copy_from_user(va_t addr, int size, void* buf);
/**
 * @brief copy bytes to userspace
 * @param addr address of user memory
 * @param size number of bytes
 * @param buf buffer
 * @return 0 on success, -1 on failure
 */
int copy_to_user(va_t addr, int size, void* buf);

/**
 * @brief try to copy a string from userspace
 * @param addr address of user memory
 * @param maxlen maximum length of the string including the null byte
 * @return NULL on failure, a string allocated by malloc() on success
 */
char* copy_string_from_user(va_t addr, int maxlen);
/**
 * @brief try to print a buffer from userspace
 * @param addr address of user memory
 * @param maxlen maximum length of to print
 * @return 0 on success, -1 on failure
 */
int print_buf_from_user(va_t addr, int maxlen);

#endif
