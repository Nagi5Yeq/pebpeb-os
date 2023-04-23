/** @file kbd.c
 *
 *  @brief Keyboard handler implementation.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <x86/asm.h>
#include <x86/interrupt_defines.h>
#include <x86/keyhelp.h>

#include <console.h>
#include <interrupt.h>
#include <kbd.h>
#include <paging.h>
#include <pv.h>
#include <sync.h>
#include <usermem.h>

/** a node in queue of threads requesting keyboard access */
typedef struct kbd_request_s {
    thread_t* t;
    queue_t node;
} kbd_request_t;

/** size of scancode ring buffer, a big buffer enough for someone pressing
 * keyboard fast */
#define KH_RING_SIZE PAGE_SIZE
/** size of character ring buffer, a big buffer enough for someone pressing
 * keyboard fast */
#define CHR_RING_SIZE PAGE_SIZE
/** maximum readline size */
#define MAX_READLINE (CHR_RING_SIZE - 1)

/** queue of threads requesting keyboard access */
static queue_t* inputs = NULL;

/** mutex for keyboard request queue */
static mutex_t kbd_request_lock = MUTEX_INIT;
/** signal for a thread leaving request queue */
static cv_t kbd_request_cv = CV_INIT;

/** mutex for new scancode input */
static mutex_t input_lock = MUTEX_INIT;
/** signal for new scancode input */
static cv_t input_cv = CV_INIT;

/** augchar buffer */
static unsigned char kh_ring[KH_RING_SIZE];
/** read pointer of augchar ring buffer */
static int kh_r_pos = 0;
/** write pointer of augchar ring buffer */
static int kh_w_pos = 0;

/** character ring buffer */
static char chr_ring[CHR_RING_SIZE];
/** read pointer of character ring buffer */
static int chr_r_pos = 0;
/** write pointer of character ring buffer */
static int chr_w_pos = 0;

/**
 * @brief keyboard inturrupt handler
 */
void kbd_handler_real(stack_frame_t* f) {
    unsigned char sc = inb(KEYBOARD_PORT);
    pic_acknowledge(KBD_IRQ);
    kh_type kh = process_scancode(sc);
    if (pv_inject_irq(f, KEY_IDT_ENTRY, kh) == 0) {
        return;
    }
    if (KH_HASDATA(kh) && KH_ISMAKE(kh)) {
        int next_w_pos = (kh_w_pos + 1) % KH_RING_SIZE;
        if (next_w_pos != kh_r_pos) {
            kh_ring[kh_w_pos] = kh;
            kh_w_pos = next_w_pos;
        }
    }
    cv_signal(&input_cv);
}

/**
 * @brief process scancode until a char is produced
 * @return char produced
 */
static int sc_process();

/**
 * @brief flush a line to user buffer
 * @param len length of buffer
 * @param buf buffer
 * @return size of chars flushed, -1: buf invalid
 */
static int flush_line(int len, va_t buf);

int do_readline(int len, va_t buf) {
    if (len < 0 || len > MAX_READLINE) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    kbd_request_t req;
    req.t = get_current();
    /* wait until our turn */
    mutex_lock(&kbd_request_lock);
    queue_insert_tail(&inputs, &req.node);
    while (inputs != &req.node) {
        cv_wait(&kbd_request_cv, &kbd_request_lock);
    }
    mutex_unlock(&kbd_request_lock);
    int should_flush = 0;
    if (chr_r_pos != chr_w_pos) {
        int i;
        for (i = chr_r_pos; i != chr_w_pos; i = (i + 1) % CHR_RING_SIZE) {
            if (chr_ring[i] == '\n') {
                should_flush = 1;
                break;
            }
        }
        /* check if ring is full */
        if (chr_r_pos == (chr_w_pos + 1) % CHR_RING_SIZE) {
            should_flush = 1;
        }
    }
    while (should_flush == 0) {
        /* wait for more chars */
        char c = sc_process();
        if (c == '\b') {
            if (chr_w_pos != chr_r_pos) {
                putbyte(c);
                chr_w_pos = (chr_w_pos + CHR_RING_SIZE - 1) % CHR_RING_SIZE;
            }
        } else {
            chr_ring[chr_w_pos] = c;
            chr_w_pos = (chr_w_pos + 1) % CHR_RING_SIZE;
            putbyte(c);
            should_flush =
                (c == '\n') || (chr_r_pos == (chr_w_pos + 1) % CHR_RING_SIZE);
        }
    }
    int result = flush_line(len, buf);
    mutex_lock(&kbd_request_lock);
    queue_detach(&inputs, &req.node);
    cv_signal(&kbd_request_cv);
    mutex_unlock(&kbd_request_lock);
    return result;
}

int do_getchar() {
    kbd_request_t req;
    req.t = get_current();
    mutex_lock(&kbd_request_lock);
    queue_insert_tail(&inputs, &req.node);
    while (inputs != &req.node) {
        cv_wait(&kbd_request_cv, &kbd_request_lock);
    }
    mutex_unlock(&kbd_request_lock);
    int result;
    if (chr_r_pos != chr_w_pos) {
        result = chr_ring[chr_r_pos];
        chr_r_pos = (chr_r_pos + 1) % CHR_RING_SIZE;
    } else {
        result = sc_process();
    }
    mutex_lock(&kbd_request_lock);
    queue_detach(&inputs, &req.node);
    cv_signal(&kbd_request_cv);
    mutex_unlock(&kbd_request_lock);
    return result;
}

static int flush_line(int len, va_t buf) {
    int size = 0, i;
    for (i = chr_r_pos; i != chr_w_pos; i = (i + 1) % CHR_RING_SIZE) {
        size++;
        if (chr_ring[i] == '\n') {
            break;
        }
    }
    if (size > len) {
        size = len;
    }
    int end_pos = (chr_r_pos + size) % CHR_RING_SIZE;
    if (end_pos > chr_r_pos) {
        if (copy_to_user(buf, size, &chr_ring[chr_r_pos]) != 0) {
            return -1;
        }
    } else {
        int size_1 = (CHR_RING_SIZE - chr_r_pos);
        if (copy_to_user(buf, size_1, &chr_ring[chr_r_pos]) != 0) {
            return -1;
        }
        if (copy_to_user(buf + size_1, end_pos, &chr_ring[0]) != 0) {
            return -1;
        }
    }
    chr_r_pos = end_pos;
    return size;
}

static int sc_process() {
    while (1) {
        /* wait for more scancode */
        mutex_lock(&input_lock);
        while (kh_r_pos == kh_w_pos) {
            cv_wait(&input_cv, &input_lock);
        }
        kh_type kh = kh_ring[kh_r_pos];
        kh_r_pos = (kh_r_pos + 1) % KH_RING_SIZE;
        mutex_unlock(&input_lock);
        return KH_GETCHAR(kh);
    }
}
