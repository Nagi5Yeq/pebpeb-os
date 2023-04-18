/*
 * Copyright (c) 1995 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _ASSERT_H_
#define _ASSERT_H_

extern void panic(const char *format, ...);

#ifndef NDEBUG
#define assert(expression) do {                                                \
    if(!(expression)) {                                                        \
        panic("%s:%u: failed assertion `%s'", __FILE__, __LINE__, #expression);\
    }                                                                          \
} while(0)
#else
#define assert(expression) ((void)0)
#endif

/* Extended for 15-410 at Carnegie Mellon.
 *
 * The following conditions are checked even if NDEBUG is defined:
 *  affirm(3 > 0);
 *  affirm_msg(3 > 0, "arithmetic broken around %d and %d", 3, 0);
 *  reject(0 > 3);
 *  reject_msg(0 > 3, "arithmetic broken around %d and %d", 0, 3);
 */

#define affirm(expression) do {                                                \
    if(!(expression)) {                                                        \
        panic("%s:%u: failed affirmation `%s'",                                \
              __FILE__, __LINE__, #expression);                                \
    }                                                                          \
} while(0)

#define reject(expression) do {                                                \
    if(expression) {                                                           \
        panic("%s:%u: rejecting result `%s'", __FILE__, __LINE__, #expression);\
    }                                                                          \
} while(0)

#define affirm_msg(expression, ...) do {                                       \
    if(!(expression)) {                                                        \
        panic(__VA_ARGS__);                                                    \
    }                                                                          \
} while(0)

#define reject_msg(expression, ...)                                            \
    affirm_msg(!(expression), __VA_ARGS__)

#endif /* _ASSERT_H_ */
