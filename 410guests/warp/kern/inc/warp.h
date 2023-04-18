#ifndef __WARP_H__
#define __WARP_H__

extern uint32_t *dmap; /* warp_map.S */

#define WARP_VADDR 0x01000000
#define WARP_PADDR 0x0100f000

void warp(void *esp, char *s, int len); /* warp.S */

#endif /* __WARP_H__ */
