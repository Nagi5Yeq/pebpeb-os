/**
 * @file malloc.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief Thread safe wrappers for malloc and family
 * @date 2023-02-22
 *
 */

#include <malloc.h>
#include <mutex.h>

mutex_t malloc_lock = MUTEX_INIT;

void* malloc(size_t size) {
    mutex_lock(&malloc_lock);
    void* result = _malloc(size);
    mutex_unlock(&malloc_lock);
    return result;
}

void* calloc(size_t nelt, size_t eltsize) {
    mutex_lock(&malloc_lock);
    void* result = _calloc(nelt, eltsize);
    mutex_unlock(&malloc_lock);
    return result;
}

void* realloc(void* buf, size_t new_size) {
    mutex_lock(&malloc_lock);
    void* result = _realloc(buf, new_size);
    mutex_unlock(&malloc_lock);
    return result;
}

void free(void* buf) {
    mutex_lock(&malloc_lock);
    _free(buf);
    mutex_unlock(&malloc_lock);
}
