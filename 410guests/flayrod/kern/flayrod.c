/** @file flayrod.c
 *  @brief Test hypervisor page-fault delivery.
 *
 *  @author Axel Feldmann (asfeldma)
 *  @author Dave Eckhardt (de0u)
 */

#include <simics.h>
#include <hvcall.h>
#include <common_kern.h>
#include <multiboot.h>

void tryit(void);

int kernel_main(mbinfo_t* mbinfo, int argc, char** argv, char** envp) {
    lprintf("BEGIN TEST: flayrod");
    tryit();
    lprintf("*****FAIL*****");
    return -1;
}
