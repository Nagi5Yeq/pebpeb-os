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

/** how many bits in a x86 byte */
#define NUM_OF_BITS_X86 8
/** Mask bits for a 8-bit byte */
#define BYTE_MASK_X86 0xFF

/** index of the register to start displaying cursor */
#define CRTC_CURSOR_START 10
/** bit to start displaying cursor */
#define CURSOR_ENABLE_BIT 0x20

/**
 * Someone is not satisfied with the type name char_t so we use this
 * to reperesent a char on screen so that nobody will mistake it with 'char'
 */
typedef struct a_char_on_screen_s {
    uint8_t ch;    /** char */
    uint8_t color; /** color */
} a_char_on_screen_t;

/** pointer to the viedo memory at B800h */
a_char_on_screen_t (*console_mem)[CONSOLE_WIDTH] =
    (a_char_on_screen_t(*)[CONSOLE_WIDTH])CONSOLE_MEM_BASE;

/** the char used to produce blank */
#define BLANK_CH ' '
/** initial color of the cursor */
#define DEFAULT_COLOR (FGND_WHITE | BGND_BLACK)

/** x position of the cursor */
static int cur_x = 0;
/** y position of the cursor */
static int cur_y = 0;
/** current color of the cursor */
static char cur_color = DEFAULT_COLOR;
/** should the cursor be displayed? */
static int cur_shown = true;

/** mutex for console operations */
mutex_t console_lock = MUTEX_INIT;

/**
 * @brief Move the cursor to a position. Position is checked before calling this
 * function
 * @param x x
 * @param y y
 */
static void move_cursor(int x, int y) {
    cur_x = x;
    cur_y = y;
    int pos = y * CONSOLE_WIDTH + x;
    outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
    outb(CRTC_DATA_REG, pos & BYTE_MASK_X86);
    outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
    outb(CRTC_DATA_REG, pos >> NUM_OF_BITS_X86);
}

/**
 * @brief Scroll the console by one line and clean the last line
 */
static void scroll_page() {
    /* move all but last line forward */
    memmove(console_mem[0], console_mem[1],
            sizeof(a_char_on_screen_t) * CONSOLE_WIDTH * (CONSOLE_HEIGHT - 1));
    int i;
    for (i = 0; i < CONSOLE_WIDTH; i++) {
        console_mem[CONSOLE_HEIGHT - 1][i] =
            (a_char_on_screen_t){BLANK_CH, cur_color};
    }
}

int putbyte(char ch) {
    if (ch == '\n') {
        if (cur_y < CONSOLE_HEIGHT - 1) {
            move_cursor(0, cur_y + 1);
        } else {
            scroll_page();
            move_cursor(0, cur_y);
        }
        return ch;
    }
    if (ch == '\r') {
        move_cursor(0, cur_y);
        return ch;
    }
    if (ch == '\b') {
        if (cur_x > 0) {
            move_cursor(cur_x - 1, cur_y);

        } else if (cur_y > 0) {
            move_cursor(CONSOLE_WIDTH - 1, cur_y - 1);
        }
        console_mem[cur_y][cur_x] = (a_char_on_screen_t){BLANK_CH, cur_color};
        return ch;
    }
    /* save the position where we should show the char */
    int prev_x = cur_x, prev_y = cur_y;
    if (cur_x < CONSOLE_WIDTH - 1) {
        /* move forward */
        move_cursor(cur_x + 1, cur_y);
    } else if (cur_y < CONSOLE_HEIGHT - 1) {
        /* go to next line */
        move_cursor(0, cur_y + 1);
    } else {
        /* scroll and go to beginning of the line */
        scroll_page();
        move_cursor(0, cur_y);
        prev_y -= 1;
    }
    console_mem[prev_y][prev_x] = (a_char_on_screen_t){ch, cur_color};
    return ch;
}

void putbytes(const char* s, int len) {
    if (s == NULL || len <= 0) {
        return;
    }
    int i;
    for (i = 0; i < len; i++) {
        putbyte(s[i]);
    }
}

void draw_char(int row, int col, int ch, int color) {
    if (row >= CONSOLE_HEIGHT || row < 0) {
        return;
    }
    if (col >= CONSOLE_WIDTH || col < 0) {
        return;
    }
    if (color > BYTE_MASK_X86 || color < 0) {
        return;
    }
    console_mem[row][col] = (a_char_on_screen_t){ch, color};
}

int set_term_color(int color) {
    if (color > BYTE_MASK_X86 || color < 0) {
        return -1;
    }
    cur_color = (char)color;
    return 0;
}

void get_term_color(int* color) {
    *color = cur_color;
}

int set_cursor(int row, int col) {
    if (row >= CONSOLE_HEIGHT || row < 0) {
        return -1;
    }
    if (col >= CONSOLE_WIDTH || col < 0) {
        return -1;
    }
    move_cursor(col, row);
    return 0;
}

void get_cursor(int* row, int* col) {
    *row = cur_y;
    *col = cur_x;
}

void hide_cursor(void) {
    cur_shown = 0;
    outb(CRTC_IDX_REG, CRTC_CURSOR_START);
    outb(CRTC_DATA_REG, inb(CRTC_DATA_REG) | CURSOR_ENABLE_BIT);
}

void show_cursor(void) {
    cur_shown = 1;
    outb(CRTC_IDX_REG, CRTC_CURSOR_START);
    outb(CRTC_DATA_REG, inb(CRTC_DATA_REG) & ~CURSOR_ENABLE_BIT);
}

void clear_console(void) {
    move_cursor(0, 0);
    int i, j;
    for (i = 0; i < CONSOLE_HEIGHT - 1; i++) {
        for (j = 0; j < CONSOLE_WIDTH - 1; j++) {
            console_mem[i][j] = (a_char_on_screen_t){BLANK_CH, cur_color};
        }
    }
}

char get_char(int row, int col) {
    if (row >= CONSOLE_HEIGHT || row < 0) {
        return BLANK_CH;
    }
    if (col >= CONSOLE_WIDTH || col < 0) {
        return BLANK_CH;
    }
    return console_mem[row][col].ch;
}
