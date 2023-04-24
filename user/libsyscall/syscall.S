#include <syscall_int.h>

.global deschedule

# int deschedule(int *flag);
deschedule:
    mov 0x4(%esp), %esi
    int $DESCHEDULE_INT
    ret

.global exec

# int exec(char *execname, char *argvec[]);
exec:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $EXEC_INT
    add $0x8, %esp
    ret

.global fork

# int fork(void);
fork:
    int $FORK_INT
    ret

.global getchar

getchar:
    int $GETCHAR_INT
    ret

.global get_cursor_pos

get_cursor_pos:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $GET_CURSOR_POS_INT
    add $0x8, %esp
    ret

.global get_ticks

get_ticks:
    int $GET_TICKS_INT
    ret

.global gettid

# int gettid(void);
gettid:
    int $GETTID_INT
    ret

.global halt

halt:
    int $HALT_INT
    ret

.global make_runnable

make_runnable:
    mov 0x4(%esp), %esi
    int $MAKE_RUNNABLE_INT
    ret

.global misbehave

misbehave:
    mov 0x4(%esp), %esi
    int $MISBEHAVE_INT
    ret

.global new_pages

new_pages:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $NEW_PAGES_INT
    add $0x8, %esp
    ret

.global print

print:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $PRINT_INT
    add $0x8, %esp
    ret

.global readfile

readfile:
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov %esp, %esi
    int $READFILE_INT
    add $0x10, %esp
    ret

.global readline

readline:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $READLINE_INT
    add $0x8, %esp
    ret

.global remove_pages

remove_pages:
    mov 0x4(%esp), %esi
    int $REMOVE_PAGES_INT
    ret

.global set_cursor_pos

set_cursor_pos:
    mov 0x8(%esp), %esi
    push %esi
    mov 0x8(%esp), %esi
    push %esi
    mov %esp, %esi
    int $SET_CURSOR_POS_INT
    add $0x8, %esp
    ret

.global set_status

# void set_status(int status);
set_status:
    mov 0x4(%esp), %esi
    int $SET_STATUS_INT
    ret

.global set_term_color

set_term_color:
    mov 0x4(%esp), %esi
    int $SET_TERM_COLOR_INT
    ret

.global sleep

sleep:
    mov 0x4(%esp), %esi
    int $SLEEP_INT
    ret

.global swexn

swexn:
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov 0x10(%esp), %esi
    push %esi
    mov %esp, %esi
    int $SWEXN_INT
    add $0x10, %esp
    ret

.global task_vanish

# void task_vanish(int status) NORETURN;
task_vanish:
    mov 0x4(%esp), %esi
    int $TASK_VANISH_INT

.global vanish

# void vanish(void) NORETURN;
vanish:
    int $VANISH_INT

.global wait

# int wait(int *status_ptr);
wait:
    mov 0x4(%esp), %esi
    int $WAIT_INT
    ret

.global yield

# int yield(int pid);
yield:
    mov 0x4(%esp), %esi
    int $YIELD_INT
    ret

.global new_console

new_console:
    int $NEW_CONSOLE_INT
    ret
