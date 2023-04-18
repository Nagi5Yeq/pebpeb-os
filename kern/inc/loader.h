/* The 15-410 kernel project
 *
 *     loader.h
 *
 * Structure definitions, #defines, and function prototypes
 * for the user process loader.
 */

#ifndef _LOADER_H
#define _LOADER_H

#include <exec2obj.h>

/* --- Prototypes --- */

/**
 * @brief read size bytes from offset of filename to buf
 * @param filename filename
 * @param offser offset in file
 * @param size size to read
 * @param buf buffer
 * @return returns the number of bytes copied on succes; -1 on failure
 */
int getbytes(const char* filename, int offset, int size, char* buf);

/** shorter name for file entry */
typedef const exec2obj_userapp_TOC_entry file_t;

/**
 * @brief read size bytes from offset of f to buf
 * @param f file entry
 * @param offser offset in file
 * @param size size to read
 * @param buf buffer
 * @return returns the number of bytes copied on succes; -1 on failure
 */
int read_file(file_t* f, int offset, int size, char* buf);

/**
 * @brief find a file entry
 * @param filename filename
 * @return the file entry, NULL if not found
 */
file_t* find_file(const char* name);

#endif /* _LOADER_H */
