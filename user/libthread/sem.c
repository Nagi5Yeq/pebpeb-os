/**
 * @file sem.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief Semaphore implementation
 * @date 2023-02-22
 * 
 */


#include <sem.h>

/**
 * @brief Initializes the semaphore pointed to by 'sem' tp value 'count'
 *
 * @param sem semaphore
 * @param count count
 * @return int
 */
int sem_init(sem_t* sem, int count) {
    sem->value = count;

    mutex_init(&sem->value_lock);
    cond_init(&sem->value_cv);

    return 0;
}

/**
 * @brief Decrement sem value, may block until it is possible to do so
 *
 * @param sem semaphore
 */
void sem_wait(sem_t* sem) {
    mutex_lock(&sem->value_lock);

    while (sem->value <= 0) {
        /* wait until value goes up */
        cond_wait(&sem->value_cv, &sem->value_lock);
    }
    sem->value -= 1;
    mutex_unlock(&sem->value_lock);
}

/**
 * @brief  wake up a thread waiting on the semaphore pointed to by sem
 *
 * @param sem semaphore
 */
void sem_signal(sem_t* sem) {
    mutex_lock(&sem->value_lock);

    sem->value += 1;

    if (sem->value == 1) {
        /* notify when value goes up from zero */
        cond_signal(&sem->value_cv);
    }
    mutex_unlock(&sem->value_lock);
}

/**
 * @brief deactivate semaphore. Must not be called on semaphores bwing waited
 *        upon
 *
 * @param sem semaphore
 */
void sem_destroy(sem_t* sem) {
    mutex_destroy(&sem->value_lock);
    cond_destroy(&sem->value_cv);
}
