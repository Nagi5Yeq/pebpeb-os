/** @file syscall_io.c
 *
 *  @brief IO syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <common_kern.h>
#include <elf/elf_410.h>
#include <string.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <console.h>
#include <kbd.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <usermem.h>

/**
 * @brief print() syscall handler
 * @param f saved regs
 */
void sys_print_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    int len;
    if (copy_from_user((va_t)esi, sizeof(int), &len) != 0) {
        goto read_fail;
    }
    if (len < 0) {
        goto bad_length;
    }
    va_t base;
    if (copy_from_user((va_t)esi + sizeof(va_t), sizeof(va_t), &base) != 0) {
        goto read_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    f->eax = (reg_t)print_buf_from_user(pts, base, len);
    mutex_unlock(&pts->lock);
    return;

read_fail:
    f->eax = (reg_t)-1;
    return;

bad_length:
    f->eax = (reg_t)-2;
}

/**
 * @brief set_term_color() syscall handler
 * @param f saved regs
 */
void sys_set_term_color_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    f->eax = (reg_t)pts_set_term_color(pts, (int)esi);
    mutex_unlock(&pts->lock);
}

/**
 * @brief set_cursor_pos() syscall handler
 * @param f saved regs
 */
void sys_set_cursor_pos_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    int row, col;
    if (copy_from_user((va_t)esi, sizeof(int), &row) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + sizeof(va_t)), sizeof(int), &col) != 0) {
        goto read_arg_fail;
    }
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    f->eax = (reg_t)pts_set_cursor(pts, row, col);
    mutex_unlock(&pts->lock);
    return;

read_arg_fail:
    f->eax = (reg_t)-1;
    return;
}

/**
 * @brief get_cursor_pos() syscall handler
 * @param f saved regs
 */
void sys_get_cursor_pos_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    va_t prow, pcol;
    if (copy_from_user((va_t)esi, sizeof(va_t), &prow) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + sizeof(va_t)), sizeof(va_t), &pcol) != 0) {
        goto read_arg_fail;
    }
    int row, col;
    pts_t* pts = get_current()->pts;
    mutex_lock(&pts->lock);
    pts_get_cursor(pts, &row, &col);
    mutex_unlock(&pts->lock);
    if (copy_to_user(prow, sizeof(int), &row) != 0) {
        goto bad_arg;
    }
    if (copy_to_user(pcol, sizeof(int), &col) != 0) {
        goto bad_arg;
    }
    f->eax = 0;
    return;

bad_arg:
read_arg_fail:
    f->eax = (reg_t)-1;
    return;
}

/**
 * @brief getchar() syscall handler
 * @param f saved regs
 */
void sys_getchar_real(stack_frame_t* f) {
    f->eax = (reg_t)do_getchar();
}

/**
 * @brief readline() syscall handler
 * @param f saved regs
 */
void sys_readline_real(stack_frame_t* f) {
    int len;
    va_t buf;
    reg_t esi = f->esi;
    if (copy_from_user((va_t)esi, sizeof(int), &len) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + sizeof(va_t)), sizeof(va_t), &buf) != 0) {
        goto read_arg_fail;
    }
    f->eax = (reg_t)do_readline(len, buf);
    return;

read_arg_fail:
    f->eax = (reg_t)-1;
    return;
}
