/**
 * The 15-410 kernel project.
 * @name loader.c
 *
 * Functions for the loading
 * of user programs from binary
 * files should be written in
 * this file. The function
 * elf_load_helper() is provided
 * for your use.
 */

/* --- Includes --- */
#include <elf_410.h>
#include <exec2obj.h>
#include <loader.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

/* --- Local function prototypes --- */

/**
 * Copies data from a file into a buffer.
 *
 * @param filename   the name of the file to copy data from
 * @param offset     the location in the file to begin copying from
 * @param size       the number of bytes to be copied
 * @param buf        the buffer to copy the data into
 *
 * @return returns the number of bytes copied on succes; -1 on failure
 */
int getbytes(const char* filename, int offset, int size, char* buf) {
    file_t* f = find_file(filename);
    if (f == NULL || offset > f->execlen) {
        return -1;
    }
    return read_file(f, offset, size, buf);
}

int read_file(file_t* f, int offset, int size, char* buf) {
    int src_size = size;
    if (f->execlen - offset < size) {
        src_size = f->execlen - offset;
    }
    memcpy(buf, f->execbytes + offset, src_size);
    return src_size;
}

file_t* find_file(const char* name) {
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++) {
        if (strcmp(name, exec2obj_userapp_TOC[i].execname) == 0) {
            return &exec2obj_userapp_TOC[i];
        }
    }
    return NULL;
}
