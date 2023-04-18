UPDATE_METHOD = afs

CC = gcc

KERNEL_OBJS = \
	warp.o warp_main.o warp_map.o \
	fake_console.o hvstubs.o
