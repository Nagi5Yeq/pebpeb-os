/*
 *
 *  #     #
 *  ##    #   ####    #####     #     ####   ######
 *  # #   #  #    #     #       #    #    #  #
 *  #  #  #  #    #     #       #    #       #####
 *  #   # #  #    #     #       #    #       #
 *  #    ##  #    #     #       #    #    #  #
 *  #     #   ####      #       #     ####   ######
 *
 * Now that it's P3 instead of P1 you are allowed
 * to edit this file if it suits you.
 *
 * Please delete this notice.
 *
 */

/** @file console.h
 *
 *  @brief console functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <x86/asm.h>
#include <x86/video_defines.h>

#include <common.h>
#include <sync.h>

/**
 * Someone is not satisfied with the type name char_t so we use this
 * to reperesent a char on screen so that nobody will mistake it with 'char'
 */
typedef struct a_char_on_screen_s {
    uint8_t ch;    /** char */
    uint8_t color; /** color */
} a_char_on_screen_t;

typedef struct pts_s {
    queue_t pts_link;
    int refcount;
    mutex_t lock;
    a_char_on_screen_t mem[CONSOLE_HEIGHT][CONSOLE_WIDTH];
    int cur_x;
    int cur_y;
    char cur_color;
    int cur_shown;
} pts_t;

extern pts_t* active_pts;
extern spl_t pts_lock;

void pts_init(pts_t* pts);

int putbyte(char ch);

/** @brief Prints character ch at the current location
 *         of the cursor.
 *
 *  If the character is a newline ('\n'), the cursor is
 *  be moved to the beginning of the next line (scrolling if necessary).  If
 *  the character is a carriage return ('\r'), the cursor
 *  is immediately reset to the beginning of the current
 *  line, causing any future output to overwrite any existing
 *  output on the line.  If backsapce ('\b') is encountered,
 *  the previous character is erased.  See the main console.c description
 *  for more backspace behavior.
 *
 *  @param ch the character to print
 *  @return The input character
 */
int pts_putbyte(pts_t* pts, char ch);

/** @brief Prints the string s, starting at the current
 *         location of the cursor.
 *
 *  If the string is longer than the current line, the
 *  string fills up the current line and then
 *  continues on the next line. If the string exceeds
 *  available space on the entire console, the screen
 *  scrolls up one line, and then the string
 *  continues on the new line.  If '\n', '\r', and '\b' are
 *  encountered within the string, they are handled
 *  as per putbyte. If len is not a positive integer or s
 *  is null, the function has no effect.
 *
 *  @param s The string to be printed.
 *  @param len The length of the string s.
 *  @return Void.
 */
void pts_putbytes(pts_t* pts, const char* s, int len);

/** @brief Changes the foreground and background color
 *         of future characters printed on the console.
 *
 *  If the color code is invalid, the function has no effect.
 *
 *  @param color The new color code.
 *  @return 0 on success or integer error code less than 0 if
 *          color code is invalid.
 */
int pts_set_term_color(pts_t* pts, int color);

/** @brief Writes the current foreground and background
 *         color of characters printed on the console
 *         into the argument color.
 *  @param color The address to which the current color
 *         information will be written.
 *  @return Void.
 */
void pts_get_term_color(pts_t* pts, int* color);

/** @brief Sets the position of the cursor to the
 *         position (row, col).
 *
 *  Subsequent calls to putbytes should cause the console
 *  output to begin at the new position. If the cursor is
 *  currently hidden, a call to set_cursor() does not show
 *  the cursor.
 *
 *  @param row The new row for the cursor.
 *  @param col The new column for the cursor.
 *  @return 0 on success or integer error code less than 0 if
 *          cursor location is invalid.
 */
int pts_set_cursor(pts_t* pts, int row, int col);

/** @brief Writes the current position of the cursor
 *         into the arguments row and col.
 *  @param row The address to which the current cursor
 *         row will be written.
 *  @param col The address to which the current cursor
 *         column will be written.
 *  @return Void.
 */
void pts_get_cursor(pts_t* pts, int* row, int* col);

#endif /* _CONSOLE_H */
