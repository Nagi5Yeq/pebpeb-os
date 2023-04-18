/** @file fake_console.c
 *  @brief Not really a console driver.
 *
 *  @author Harry Q. Bovik (hqbovik)
 *  @author Jack Duvall (jrduvall)
 */

#include <types.h>
#include <console.h>
#include <hvcall.h>

int putbyte(char ch) {
  hv_print(1, (unsigned char*)&ch);
  return ch;
}

void putbytes(const char* s, int len) {
  hv_print(len, (unsigned char *)(size_t)s);
}

int set_term_color(int color) {
  return (-1);
}

void get_term_color(int* color) {}

int set_cursor(int row, int col) {
  return (-1);
}

void get_cursor(int* row, int* col) {}

void hide_cursor() {}

void show_cursor() {}

void clear_console() {}

void draw_char(int row, int col, int ch, int color) {}

char get_char(int row, int col) {
  return ' ';
}
