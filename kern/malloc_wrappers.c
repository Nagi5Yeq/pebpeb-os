/** @file malloc_wrappers.c
 *
 *  @brief locked malloc functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <malloc.h>
#include <malloc_internal.h>
#include <stddef.h>

#include <sync.h>

mutex_t malloc_lock = MUTEX_INIT;

/* safe versions of malloc functions */
void* malloc(size_t size) {
    mutex_lock(&malloc_lock);
    void* mem = _malloc(size);
    mutex_unlock(&malloc_lock);
    return mem;
}

void* memalign(size_t alignment, size_t size) {
    mutex_lock(&malloc_lock);
    void* mem = _memalign(alignment, size);
    mutex_unlock(&malloc_lock);
    return mem;
}

void* calloc(size_t nelt, size_t eltsize) {
    mutex_lock(&malloc_lock);
    void* mem = _calloc(nelt, eltsize);
    mutex_unlock(&malloc_lock);
    return mem;
}

void* realloc(void* buf, size_t new_size) {
    mutex_lock(&malloc_lock);
    void* mem = _realloc(buf, new_size);
    mutex_unlock(&malloc_lock);
    return mem;
}

void free(void* buf) {
    mutex_lock(&malloc_lock);
    _free(buf);
    mutex_unlock(&malloc_lock);
}

void* smalloc(size_t size) {
    mutex_lock(&malloc_lock);
    void* mem = _smalloc(size);
    mutex_unlock(&malloc_lock);
    return mem;
}

void* smemalign(size_t alignment, size_t size) {
    mutex_lock(&malloc_lock);
    void* mem = _smemalign(alignment, size);
    mutex_unlock(&malloc_lock);
    return mem;
}

void sfree(void* buf, size_t size) {
    mutex_lock(&malloc_lock);
    _sfree(buf, size);
    mutex_unlock(&malloc_lock);
}
