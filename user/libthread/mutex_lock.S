#include <syscall_int.h>

// it is impossible to implement mutex_unlock_vanish without stack in c
// so we implement all lock functions in assembly and mutex_unlock can
// share function body with mutex_unlock_vanish

.mutex_unlock_body:
    xor %eax, %eax                 //
    lock xchg %eax, (%ebp)         // mp->locked = 0;
.mutex_unlock_try:                 // while(true) {
    xor %eax, %eax                 //
    mov $1, %edi                   //
    lock cmpxchg %edi, 0x4(%ebp)   //    eax = compare_and_set(&mp->w_lock, 1);
    test %eax, %eax                //    if(eax == 0) {break;}
    jnz .mutex_unlock_try          // }
    mov 0x8(%ebp), %edi            // edi = mp->w_list;
    test %edi, %edi                //
    jz .mutex_unlock_w_empty       // if(edi != NULL) {
    mov $1, %edx                   //
    mov %edx, 0x28(%edi)           //    edi->mutex_resume = 1;
    mov (%edi), %esi               //
    int $MAKE_RUNNABLE_INT         //    make_runnable(edi->tid);
.mutex_unlock_w_empty:             // }
    xor %eax, %eax                 //
    lock xchg %eax, 0x4(%ebp)      // mp->w_lock = 0;
    jmp *%ebx

.global mutex_unlock
.type mutex_unlock, %function
mutex_unlock:
    push %ebx
    push %ebp
    mov 0xc(%esp), %ebp            // ebp = mp
    lea .mutex_unlock_done, %ebx   // ebx will be used by mutex_unlock_body as return address
    jmp .mutex_unlock_body
.mutex_unlock_done:
    pop %ebp
    pop %ebx
    ret

.global mutex_unlock_vanish
.type mutex_unlock_vanish, %function
mutex_unlock_vanish:
    mov 0x4(%esp), %ebp
    lea .mutex_unlock_do_vanish, %ebx
    jmp .mutex_unlock_body
.mutex_unlock_do_vanish:
    int $VANISH_INT

.global mutex_lock
.type mutex_lock, %function
mutex_lock:
    push %ebx                      //
    push %ebp                      //
    call get_self_tcb              //
    mov %eax, %ebx                 // ebx = get_self_tcb();
    xor %eax, %eax                 //
    mov %eax, 0x28(%ebx)           // ebx->mutex_resume = 0;
    mov 0xc(%esp), %ebp            // ebp = mp;
.mutex_lock_w_lock:                // while(true) {
    xor %eax, %eax                 //
    mov $1, %edi                   //
    lock cmpxchg %edi, 0x4(%ebp)   //    eax = compare_and_set(&mp->w_lock, 1);
    test %eax, %eax                //    if(eax == 0) {break;}
    jnz .mutex_lock_w_lock         // }
    mov 0x8(%ebp), %edi            // edi = mp->w_list;
    test %edi, %edi                //
    jz .mutex_lock_w_empty         // if(edi != NULL) {
    mov %edi, 0x20(%ebx)           //    ebx->mutex_next = edi;
    mov 0x24(%edi), %edx           //    edx = edi->mutex_prev;
    mov %edx, 0x24(%ebx)           //    ebx->mutex_prev = edx;
    mov %ebx, 0x20(%edx)           //    edx->mutex_next = ebx;
    mov %ebx, 0x24(%edi)           //    edi->mutex_prev = ebx;
    jmp .mutex_lock_w_done         // }
.mutex_lock_w_empty:               // else {
    mov %ebx, 0x8(%ebp)            //    mp->w_list = ebx;
    mov %ebx, 0x20(%ebx)           //    ebx->mutex_next = ebx;
    mov %ebx, 0x24(%ebx)           //    ebx->mutex_prev = ebx;
.mutex_lock_w_done:                // }
    lock xchg %eax, 0x4(%ebp)      // mp->w_lock = 0;
.mutex_lock_try:                   //
    xor %eax, %eax                 // while(true) {
    mov $1, %edi                   //
    lock cmpxchg %edi, (%ebp)      //    eax = compare_and_set(&mp->locked, 1);
    test %eax, %eax                //
    jz .mutex_lock_success         //    if(eax == 0) {break;}
    lea 0x28(%ebx), %eax           //
    push %eax                      //
    call deschedule                //    deschedule(&ebx->mutex_resume);
    add $0x4, %esp                 //
    jmp .mutex_lock_try            // }
.mutex_lock_success:               // while(true) {
    xor %eax, %eax                 //
    mov $1, %edi                   //
    lock cmpxchg %edi, 0x4(%ebp)   //    eax = compare_and_set(&mp->w_lock, 1);
    test %eax, %eax                //    if(eax == 0) {break;}
    jnz .mutex_lock_success        // }
    mov 0x20(%ebx), %edi           // edi = ebx->mutex_next;
    mov 0x24(%ebx), %edx           // edx = ebx->mutex_prev;
    cmp %edi, %ebx                 //
    je .mutex_lock_w_last          // if(edi != ebx) {
    mov %edx, 0x24(%edi)           //    edi->mutex_prev = edx;
    mov %edi, 0x20(%edx)           //    edx->mutex_next = edi;
    cmp %ebx, 0x8(%ebp)            //
    jne .mutex_lock_w_end          //    if(ebx == mp->w_list)
    mov %edi, 0x8(%ebp)            //       mp->w_list = edi;
    jmp .mutex_lock_w_end          // }
.mutex_lock_w_last:                // else {
    mov %eax, 0x8(%ebp)            //    mp->w_list = NULL;
.mutex_lock_w_end:                 // }
    lock xchg %eax, 0x4(%ebp)      // mp->w_lock = 0;
    pop %ebp
    pop %ebx
    ret
