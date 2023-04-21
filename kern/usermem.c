/** @file usermem.c
 *
 *  @brief userspace r/w functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <stdlib.h>
#include <string.h>

#include <common_kern.h>
#include <x86/asm.h>

#include <console.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>

/**
 * @brief try to read a byte from userspace
 * @param addr address
 * @param output output byte if successful
 * @return 0 on success, jump to usermem_fail on failure
 */
int try_read(va_t addr, char* output);
/**
 * @brief try to write a byte to userspace
 * @param addr address
 * @param c byte to write
 * @return 0 on success, jump to usermem_fail on failure
 */
int try_write(va_t addr, char c);

/**
 * @brief tell the calling function the r/w failed
 * @return -1
 */
int usermem_fail();

/**
 * @brief setup fault handler before r/w
 * @return old fault handler
 */
static reg_t usermem_setup() {
    thread_t* current = get_current();
    reg_t old_eip0 = current->eip0;
    current->eip0 = (reg_t)usermem_fail;
    return old_eip0;
}

/**
 * @brief restore old fault handler
 * @param old_eip0 old fault handler
 */
static void usermem_finish(reg_t old_eip0) {
    get_current()->eip0 = old_eip0;
}

int copy_from_user(va_t addr, int size, void* buf) {
    reg_t old_eip0 = usermem_setup();
    int i, result = 0;
    for (i = 0; i < size; i++) {
        if (try_read(addr + i, (char*)buf + i) != 0) {
            result = -1;
            break;
        }
    }
    usermem_finish(old_eip0);
    return result;
}

int copy_to_user(va_t addr, int size, void* buf) {
    reg_t old_eip0 = usermem_setup();
    int i, result = 0;
    for (i = 0; i < size; i++) {
        if (try_write(addr + i, *((char*)buf + i)) != 0) {
            result = -1;
            break;
        }
    }
    usermem_finish(old_eip0);
    return result;
}

char* copy_string_from_user(va_t addr, int maxlen) {
    reg_t old_eip0 = usermem_setup();
    int len = 0, buflen = 3 * sizeof(int);
    char* buf = malloc(buflen);
    char c;
    if (buf == NULL) {
        goto read_string_finished;
    }
    while (1) {
        if (len >= maxlen || try_read(addr + len, &c) != 0) {
            free(buf);
            buf = NULL;
            break;
        }

        buf[len++] = c;
        if (c == '\0') {
            break;
        }

        if (len >= buflen) {
            buflen <<= 1;
            char* result = realloc(buf, buflen);
            if (result == NULL) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = result;
        }
    }
read_string_finished:
    usermem_finish(old_eip0);
    return buf;
}

int print_buf_from_user(va_t addr, int len) {
    reg_t old_eip0 = usermem_setup();
    int i, result = 0;
    char c;
    for (i = 0; i < len; i++) {
        if (try_read(addr + i, &c) != 0) {
            result = -1;
            break;
        }
        putbyte(c);
    }
    usermem_finish(old_eip0);
    return result;
}
