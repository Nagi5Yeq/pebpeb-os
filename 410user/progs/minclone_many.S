/**
 * @file minclone_many.S
 * @author nwf
 * @author de0u
 * @brief Spawn many threads within a task:
 *        128 blocks of 64 threads, with
 *        occasional pauses for cleanup.
 * @public yes
 * @for p3
 */

#include "410_tests.h"
#include <syscall_int.h>

.section .rodata

test_name:
        .string "minclone_many:"
str3str:
        .string "%s%s%s"
str_thread:
        .string "%s: hello from tid %d in block %d"
test_start:
        .string TEST_START_CMPLT
test_error:
        .string TEST_END_FAIL
test_finish:
        .string TEST_END_SUCCESS
test_pfx:
        .string TEST_PFX

line_fmt:
        .string "%02X "

str_T:
        .string "T"
str_bang:
        .string "!"
str_newline:
        .string "\n"
.align 4
/* syscall packets for print() */
print_T_args:
        .long 1
        .long str_T
print_bang_args:
        .long 1
        .long str_bang
print_newline_args:
        .long 1
        .long str_newline

.section .text

/* Created threads avoid using the original thread's stack.
 * XXX Arguably better: array hosting thread stacks.
 */

.global main
main:
        pushl   %ebp
        movl    %esp,%ebp

        /* REPORT_START_CMPLT */
        pushl   $test_start
        pushl   $test_name
        pushl   $test_pfx
        pushl   $str3str
        call    lprintf
        movl    %ebp,%esp

        /* set_status(-17) */
        movl    $-17,%esi
        int     $SET_STATUS_INT

        /* Engage grading-harness progress sensor */
        /* sim_fr_prog(30) */
        movl    $30, %ecx
        movl    $SIM_FR_PROG, %ebx
        xchg    %bx, %bx

        movl    $128, %edx   /* %edx = block count */

block_loop:
        /* printf("%02x ", block) */
        pushl   %ecx
        pushl   %edx
        pushl   $line_fmt
        call    printf
        popl    %eax
        popl    %edx
        popl    %ecx

        movl    $64, %ecx    /* %ecx = per-block thread count */

thread_loop:
        int     $THREAD_FORK_INT
        cmpl    $0x0, %eax
        jg      parent_thread
        jl      error

spawned_thread:
        /* Declare progress to grading harness */
        /* sim_fr_here() */
        movl    $SIM_FR_HERE, %ebx
        xchg    %bx, %bx

        /* print(1, "T") */
        movl    $print_T_args, %esi
        int     $PRINT_INT

        int     $VANISH_INT

parent_thread:
        decl    %ecx
        jne     thread_loop

        /* sleep(7) -- hopefully enough for the threads to finish */
        movl    $7,%esi
        int     $SLEEP_INT

        /* print(1, "\n") */
        movl    $print_newline_args, %esi
        int     $PRINT_INT

        decl    %edx
        jne     block_loop

finish:
        /* REPORT_END_SUCCESS */
        pushl   $test_finish
        pushl   $test_name
        pushl   $test_pfx
        pushl   $str3str
        call    lprintf
        movl    %ebp,%esp
        pushl   $1
        pushl   $test_name
        call    sim_update_scoreboard
        movl    %ebp,%esp

        /* set_status(0) */
        movl    $0x0,%esi
        int     $SET_STATUS_INT

        int     $VANISH_INT

error:
        /* print(1, "!") */
        movl    $print_bang_args, %esi
        int     $PRINT_INT

        /* sleep(7) -- hopefully drain out broken partial block of threads */
        movl    $7,%esi
        int     $SLEEP_INT

        /* print(1, "\n") */
        movl    $print_newline_args, %esi
        int     $PRINT_INT

        /* REPORT_END_FAIL */
        pushl   $test_error
        pushl   $test_name
        pushl   $test_pfx
        pushl   $str3str
        call    lprintf
        call    printf
        movl    $print_newline_args, %esi
        int     $PRINT_INT
        movl    %ebp,%esp
        pushl   $0
        pushl   $test_name
        call    sim_update_scoreboard
        movl    %ebp,%esp

        int     $VANISH_INT

