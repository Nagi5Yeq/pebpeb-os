/** @file console.c
 *
 *  @brief Console driver implementation.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <x86/asm.h>
#include <x86/video_defines.h>

#include <console.h>
#include <sched.h>

/** index of the register to start displaying cursor */
#define CRTC_CURSOR_START 10
/** bit to start displaying cursor */
#define CURSOR_ENABLE_BIT 0x20

/** pointer to the viedo memory at B800h */
a_char_on_screen_t (*console_mem)[CONSOLE_WIDTH] =
    (a_char_on_screen_t(*)[CONSOLE_WIDTH])CONSOLE_MEM_BASE;

/** the char used to produce blank */
#define BLANK_CH ' '
/** initial color of the cursor */
#define DEFAULT_COLOR (FGND_WHITE | BGND_BLACK)

pts_t* active_pts = NULL;
spl_t pts_lock = SPL_INIT;
queue_t* all_pts = NULL;

void pts_init(pts_t* pts) {
    pts->cur_x = pts->cur_y = 0;
    pts->cur_color = DEFAULT_COLOR;
    pts->cur_shown = 1;
    pts->lock = MUTEX_INIT;
    pts->refcount = 0;
    queue_insert_head(&all_pts, &pts->pts_link);
}

/**
 * @brief Move the cursor to a position. Position is checked before calling this
 * function
 * @param x x
 * @param y y
 */
static inline void move_cursor(pts_t* pts, int x, int y) {
    pts->cur_x = x;
    pts->cur_y = y;
    int old_if = spl_lock(&pts_lock);
    if (active_pts == pts) {
        int pos = y * CONSOLE_WIDTH + x;
        outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
        outb(CRTC_DATA_REG, pos & 0xff);
        outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
        outb(CRTC_DATA_REG, pos >> 8);
    }
    spl_unlock(&pts_lock, old_if);
}

/**
 * @brief Scroll the console by one line and clean the last line
 */
static inline void scroll_page(pts_t* pts) {
    /* move all but last line forward */
    memmove(pts->mem[0], pts->mem[1],
            sizeof(a_char_on_screen_t) * CONSOLE_WIDTH * (CONSOLE_HEIGHT - 1));
    int i;
    for (i = 0; i < CONSOLE_WIDTH; i++) {
        pts->mem[CONSOLE_HEIGHT - 1][i] =
            (a_char_on_screen_t){BLANK_CH, pts->cur_color};
    }
    int old_if = spl_lock(&pts_lock);
    if (active_pts == pts) {
        memmove(
            console_mem[0], console_mem[1],
            sizeof(a_char_on_screen_t) * CONSOLE_WIDTH * (CONSOLE_HEIGHT - 1));
        int i;
        for (i = 0; i < CONSOLE_WIDTH; i++) {
            console_mem[CONSOLE_HEIGHT - 1][i] =
                (a_char_on_screen_t){BLANK_CH, pts->cur_color};
        }
    }
    spl_unlock(&pts_lock, old_if);
}

static inline void draw_char(pts_t* pts,
                             int row,
                             int col,
                             a_char_on_screen_t ch) {
    pts->mem[row][col] = ch;
    int old_if = spl_lock(&pts_lock);
    if (active_pts == pts) {
        console_mem[row][col] = ch;
    }
    spl_unlock(&pts_lock, old_if);
}

int putbyte(char ch) {
    return pts_putbyte(get_current()->pts, ch);
}

int pts_putbyte(pts_t* pts, char ch) {
    if (ch == '\n') {
        if (pts->cur_y < CONSOLE_HEIGHT - 1) {
            move_cursor(pts, 0, pts->cur_y + 1);
        } else {
            scroll_page(pts);
            move_cursor(pts, 0, pts->cur_y);
        }
        return ch;
    }
    if (ch == '\r') {
        move_cursor(pts, 0, pts->cur_y);
        return ch;
    }
    if (ch == '\b') {
        if (pts->cur_x > 0) {
            move_cursor(pts, pts->cur_x - 1, pts->cur_y);

        } else if (pts->cur_y > 0) {
            move_cursor(pts, CONSOLE_WIDTH - 1, pts->cur_y - 1);
        }
        a_char_on_screen_t c = {BLANK_CH, pts->cur_color};
        draw_char(pts, pts->cur_y, pts->cur_x, c);
        return ch;
    }
    /* save the position where we should show the char */
    int prev_x = pts->cur_x, prev_y = pts->cur_y;
    if (pts->cur_x < CONSOLE_WIDTH - 1) {
        /* move forward */
        move_cursor(pts, pts->cur_x + 1, pts->cur_y);
    } else if (pts->cur_y < CONSOLE_HEIGHT - 1) {
        /* go to next line */
        move_cursor(pts, 0, pts->cur_y + 1);
    } else {
        /* scroll and go to beginning of the line */
        scroll_page(pts);
        move_cursor(pts, 0, pts->cur_y);
        prev_y -= 1;
    }
    a_char_on_screen_t c = {ch, pts->cur_color};
    draw_char(pts, prev_y, prev_x, c);
    return ch;
}

void pts_putbytes(pts_t* pts, const char* s, int len) {
    if (s == NULL || len <= 0) {
        return;
    }
    int i;
    for (i = 0; i < len; i++) {
        pts_putbyte(pts, s[i]);
    }
}

int pts_set_term_color(pts_t* pts, int color) {
    if (color > 0xff || color < 0) {
        return -1;
    }
    pts->cur_color = (char)color;
    return 0;
}

void pts_get_term_color(pts_t* pts, int* color) {
    *color = pts->cur_color;
}

int pts_set_cursor(pts_t* pts, int row, int col) {
    if (row >= CONSOLE_HEIGHT || row < 0) {
        return -1;
    }
    if (col >= CONSOLE_WIDTH || col < 0) {
        return -1;
    }
    move_cursor(pts, col, row);
    return 0;
}

void pts_get_cursor(pts_t* pts, int* row, int* col) {
    *row = pts->cur_y;
    *col = pts->cur_x;
}
