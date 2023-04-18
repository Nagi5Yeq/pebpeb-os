/** @file asm_instr.h
 *
 *  @brief assembly stubs.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _ASM_INSTR_H_
#define _ASM_INSTR_H_

#include <paging.h>

/**
 * @brief runs invlpg on an address
 * @param va addr to invalidate
 */
void invlpg(va_t va);

/**
 * @brief runs hlt
 */
void hlt();

#endif
