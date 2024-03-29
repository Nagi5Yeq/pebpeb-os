###########################################################################
#
#    #####          #######         #######         ######            ###
#   #     #            #            #     #         #     #           ###
#   #                  #            #     #         #     #           ###
#    #####             #            #     #         ######             #
#         #            #            #     #         #
#   #     #            #            #     #         #                 ###
#    #####             #            #######         #                 ###
#
#
# Please read the directions in README and in this config.mk carefully.
# Do -N-O-T- just dump things randomly in here until your kernel builds.
# If you do that, you run an excellent chance of turning in something
# which can't be graded.  If you think the build infrastructure is
# somehow restricting you from doing something you need to do, contact
# the course staff--don't just hit it with a hammer and move on.
#
# [I REFUSE TO READ IT]
###########################################################################

###########################################################################
# This is the include file for the make file.
# You should have to edit only this file to get things to build.
###########################################################################

###########################################################################
# Tab stops
###########################################################################
# If you use tabstops set to something other than the international
# standard of eight characters, this is your opportunity to inform
# our print scripts.
TABSTOP = 8

###########################################################################
# Compiler
###########################################################################
# Selections (see handout for details):
#
# gcc - default (what we will grade with)
# clang - Clang/LLVM
# clangalyzer - Clang/LLVM plus static analyzer
#
# "gcc" may have a better Simics debugging experience
#
# "clang" may provide helpful warnings, assembly
# code may be more readable
#
# "clangalyzer" will likely complain more than "clang"
#
# Use "make veryclean" if you adjust CC.
CC = gcc

###########################################################################
# DEBUG
###########################################################################
# You can set CONFIG_DEBUG to any mixture of the words
# "kernel" and "user" to #define the DEBUG flag when
# compiling the respective type(s) of code.  The ordering
# of the words doesn't matter, and repeating a word has
# no additional effect.
#
# Use "make veryclean" if you adjust CONFIG_DEBUG.
#
CONFIG_DEBUG = user kernel

###########################################################################
# NDEBUG
###########################################################################
# You can set CONFIG_NDEBUG to any mixture of the words
# "kernel" and "user" to #define the NDEBUG flag when
# compiling the respective type(s) of code.  The ordering
# of the words doesn't matter, and repeating a word has
# no additional effect.  Defining NDEBUG will cause the
# checks using assert() to be *removed*.
#
# Use "make veryclean" if you adjust CONFIG_NDEBUG.
#
CONFIG_NDEBUG =

###########################################################################
# The method for acquiring project updates.
###########################################################################
# This should be "afs" for any Andrew machine, "web" for non-andrew machines
# and "offline" for machines with no network access.
#
# "offline" is strongly not recommended as you may miss important project
# updates.
#
UPDATE_METHOD = afs

###########################################################################
# WARNING: When we test your code, the two TESTS variables below will be
# blanked.  Your kernel MUST BOOT AND RUN if 410TESTS and STUDENTTESTS
# are blank.  It would be wise for you to test that this works.
###########################################################################

###########################################################################
# Test programs provided by course staff you wish to run
###########################################################################
# A list of the test programs you want compiled in from the 410user/progs
# directory.
#
410TESTS =

###########################################################################
# Test programs you have written which you wish to run
###########################################################################
# A list of the test programs you want compiled in from the user/progs
# directory.
#
STUDENTTESTS = mandelbrot racer nibbles

###########################################################################
# Data files provided by course staff to build into the RAM disk
###########################################################################
# A list of the data files you want built in from the 410user/files
# directory.
#
410FILES =

###########################################################################
# Data files you have created which you wish to build into the RAM disk
###########################################################################
# A list of the data files you want built in from the user/files
# directory.
#
STUDENTFILES =

###########################################################################
# Object files for your thread library
###########################################################################
THREAD_OBJS = malloc.o panic.o thread.o thread_fork.o mutex.o cond.o sem.o \
 rwlock.o mutex_lock.o rb_tcb.o

# Thread Group Library Support.
#
# Since libthrgrp.a depends on your thread library, the "buildable blank
# P3" we give you can't build libthrgrp.a.  Once you install your thread
# library and fix THREAD_OBJS above, uncomment this line to enable building
# libthrgrp.a:
410USER_LIBS_EARLY += libthrgrp.a

###########################################################################
# Object files for your syscall wrappers
###########################################################################
SYSCALL_OBJS = syscall.o

###########################################################################
# Object files for your automatic stack handling
###########################################################################
AUTOSTACK_OBJS = autostack.o

###########################################################################
# Parts of your kernel
###########################################################################
#
# Kernel object files you want included from 410kern/
#
410KERNEL_OBJS = load_helper.o
#
# Kernel object files you provide in from kern/
#
KERNEL_OBJS = pts.o kernel.o loader.o malloc_wrappers.o sync_asm.o \
	mm.o asm_instr.o sched.o sched_asm.o syscall_asm.o interrupt.o \
	paging.o timer.o interrupt_asm.o usermem.o syscall_process.o \
	syscall_memory.o syscall_thread.o common.o sync.o syscall_io.o \
	usermem_asm.o syscall_misc.o pv.o hvcall.o toad.o timer_asm.o

###########################################################################
# WARNING: Do not put **test** programs into the REQPROGS variables.  Your
#          kernel will probably not build in the test harness and you will
#          lose points.
###########################################################################

###########################################################################
# Mandatory programs whose source is provided by course staff
###########################################################################
# A list of the programs in 410user/progs which are provided in source
# form and NECESSARY FOR THE KERNEL TO RUN.
#
# The shell is a really good thing to keep here.  Don't delete idle
# or init unless you are writing your own, and don't do that unless
# you have a really good reason to do so.
#
410REQPROGS = \
	ack fib mem_eat_test sleep_test1 \
	actual_wait fork_bomb mem_permissions stack_test1 \
	bg fork_exit_bomb merchant swexn_basic_test \
	bistromath fork_test1 minclone_mem swexn_cookie_monster \
	cat fork_wait_bomb new_pages swexn_dispatch \
	cho2 fork_wait peon swexn_rampage \
	cho getpid_test1 print_basic swexn_regs \
	cho_variant halt_test swexn_stands_for_swextensible \
	chow idle readline_basic swexn_uninstall_test \
	ck1 init register_test wait_getpid \
	coolness knife remove_pages_test1 wild_test1 \
	deschedule_hang loader_test1 remove_pages_test2 work \
	exec_basic loader_test2 score yield_desc_mkrun \
	exec_basic_helper make_crash shell \
	exec_nonexist make_crash_helper slaughter \
	minclone_many new_shell


###########################################################################
# Mandatory programs whose source is provided by you
###########################################################################
# A list of the programs in user/progs which are provided in source
# form and NECESSARY FOR THE KERNEL TO RUN.
#
# Leave this blank unless you are writing custom init/idle/shell programs
# (not generally recommended).  If you use STUDENTREQPROGS so you can
# temporarily run a special debugging version of init/idle/shell, you
# need to be very sure you blank STUDENTREQPROGS before turning your
# kernel in, or else your tweaked version will run and the test harness
# won't.
#
STUDENTREQPROGS =

410GUESTBINS = hello magic console station tick_tock dog gomoku flayrod \
	cliff teeny vast warp fondle dumper pathos
