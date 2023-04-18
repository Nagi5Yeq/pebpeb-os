/**
 * @file mutex.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief Mutex implementation
 * @date 2023-02-22
 *
 */

#include <stdint.h>

#include <assert.h>
#include <mutex.h>

/**
 * @brief initialize the mutex pointed by mp
 *
 * @param mp mutex
 * @return 0
 */
int mutex_init(mutex_t* mp) {
    mp->locked = 0;
    mp->w_lock = 0;
    mp->w_list = NULL;
    return 0;
}

/**
 * @brief deactivate the mutex pointed by mp
 *
 * @param mp mutex
 */
void mutex_destroy(mutex_t* mp) {
    affirm(mp->locked == 0);
}
