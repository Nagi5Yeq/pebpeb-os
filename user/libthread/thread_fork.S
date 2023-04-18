#include <syscall_int.h>

// int thread_fork(tcb_t* tcb,
//                 void* (*f)(void*),
//                 void* args);
.global thread_fork
.type thread_fork, %function
thread_fork:
    mov 0x4(%esp), %edi   // tcb
    mov 0x8(%esp), %esi   // f
    mov 0xc(%esp), %edx   // arg
    int $THREAD_FORK_INT
    cmp $0, %eax
    jne .parent           // both failure and success returns non-zero for parent
    mov 0x8(%edi), %esp   // read tcb.stack_hi and set stack
    push %edx
    push %esi
    call thr_begin        // thread entry point and this will not return
.parent:
    ret

.global get_esp
.type get_esp, %function
get_esp:
    lea -0x4(%esp), %eax
    ret
