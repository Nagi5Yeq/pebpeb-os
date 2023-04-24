/** @file syscall_misc.c
 *
 *  @brief miscellaneous syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <stdio.h>
#include <string.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <malloc_internal.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <assert.h>
#include <loader.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>
#include <toad.h>
#include <usermem.h>

/**
 * @brief misbehave() syscall handler
 * @param f saved regs
 */
void sys_misbehave_real(stack_frame_t* f) {
    f->eax = 0;
}

/**
 * @brief halt() syscall handler
 * @param f saved regs
 */
void sys_halt_real(stack_frame_t* f) {
    sim_halt();
    hlt();
}

/**
 * @brief read the "." file to user buf
 * @param buf buffer
 * @param count size
 * @param offset offset
 * @return number of bytes read, or -1 on failure
 */
static reg_t read_dot_file(va_t buf, int count, int offset);

/**
 * @brief readfile() syscall handler
 * @param f saved regs
 */
void sys_readfile_real(stack_frame_t* f) {
    reg_t esi = f->esi;
    va_t pfilename, buf;
    int count, offset;
    if (copy_from_user((va_t)esi, sizeof(va_t), &pfilename) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + sizeof(va_t)), sizeof(va_t), &buf) != 0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + 2 * sizeof(va_t)), sizeof(int), &count) !=
        0) {
        goto read_arg_fail;
    }
    if (copy_from_user((va_t)(esi + 3 * sizeof(va_t)), sizeof(int), &offset) !=
        0) {
        goto read_arg_fail;
    }
    if (count < 0 || offset < 0) {
        goto read_arg_fail;
    }
    char* filename = copy_string_from_user(pfilename, MAX_EXECNAME_LEN);
    if (filename == NULL) {
        goto read_arg_fail;
    }
    if (strcmp(filename, ".") == 0) {
        free(filename);
        f->eax = read_dot_file(buf, count, offset);
        return;
    }
    file_t* fp = find_file(filename);
    if (fp == NULL) {
        goto no_such_file;
    }
    if (offset > fp->execlen) {
        goto offset_too_big;
    }
    int size = fp->execlen - offset;
    if (count < size) {
        size = count;
    }
    if (copy_to_user(buf, size, (void*)(fp->execbytes + offset)) != 0) {
        goto bad_buffer;
    }
    f->eax = (reg_t)size;
    return;

bad_buffer:
offset_too_big:
no_such_file:
    free(filename);
read_arg_fail:
    f->eax = (reg_t)-1;
    return;
}

static reg_t read_dot_file(va_t buf, int count, int offset) {
    file_t* fp;
    int i = 0;
    int cur = 0;
    int written = 0, left = count;
    while (i < exec2obj_userapp_count && left > 0) {
        fp = &exec2obj_userapp_TOC[i];
        int len = strlen(fp->execname) + 1;
        if (cur + len <= offset) {
            /* before offset */
            cur += len;
            i++;
            continue;
        }
        int size = len;
        const char* start = fp->execname;
        /* write this entry to buffer */
        if (cur < offset) {
            size -= (offset - cur);
            start += (offset - cur);
        }
        if (left < size) {
            size = left;
        }
        if (copy_to_user((va_t)buf + written, size, (void*)start) != 0) {
            return (reg_t)-1;
        }
        cur += len;
        written += size;
        left -= size;
        i++;
    }
    /* write extra null byte */
    if (left != 0 && cur == offset) {
        char nul = '\0';
        if (copy_to_user((va_t)buf + written, 1, &nul) != 0) {
            return (reg_t)-1;
        }
        written++;
    }
    return (reg_t)written;
}

void sys_new_console_real(stack_frame_t* f) {
    thread_t* t = get_current();
    pts_t* old_pts = t->pts;
    pts_t* pts = smalloc(sizeof(pts_t));
    if (pts == NULL) {
        f->eax = (reg_t)-1;
        return;
    }
    pts_init(pts);

    pts->refcount++;
    t->pts = pts;
    mutex_lock(&old_pts->lock);
    old_pts->refcount--;
    mutex_unlock(&old_pts->lock);
    switch_pts(pts);
    print_toad();
}
