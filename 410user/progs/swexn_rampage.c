/**
 * @file swexn_rampage.c
 * @author de0u
 * @brief Tests a simple use case for swexn
 * @public yes
 * @for p3
 */

#include <stddef.h>
#include <stdlib.h>
#include <syscall.h>
#include "410_tests.h"

#define USER_MEM_START 0x01000000
#define KERNEL_BOTTOM  0x00100000 // 1 megabyte; below there lie dragons

#define ENOUGH 410

DEF_TEST_NAME("swexn_rampage:");

#define STAQ_SIZE (4096)
static char exn_staq[STAQ_SIZE];
#define EXN_STAQ_TOP ((void *)(&exn_staq[STAQ_SIZE-7]))

void install(void);
void rampage(void);
void handler(void *arg, ureg_t *uregs);

static unsigned int *top_addr = (unsigned int *)USER_MEM_START - sizeof (unsigned int);

static int events = 0;

static unsigned int start_ticks;

void install(void)
{
    int ret;

    ret = swexn(EXN_STAQ_TOP, handler, NULL, NULL);
    if (ret < 0) {
        REPORT_MISC("swexn call rejected");
        REPORT_END_FAIL;
        exit(-1);
    }
}

void rampage(void)
{
    unsigned int *ip;

    for (ip = top_addr; ip > (unsigned int *)KERNEL_BOTTOM; ip--)
        *ip = 0;

    REPORT_END_FAIL; // as if
}

void handler(void *arg, ureg_t *uregs)
{
    if (uregs->cause != SWEXN_CAUSE_PAGEFAULT) {
        report_fmt("bad cause %d", uregs->cause);
        REPORT_END_FAIL;
        exit(-1);
    }
    if (uregs->cr2 != (unsigned int) top_addr) {
        report_fmt("bad addr 0x%x", uregs->cr2);
        REPORT_END_FAIL;
        exit(-1);
    }

    TEST_PROG_PROGRESS;

    if (++events == ENOUGH) {
        report_fmt("ticks %d", get_ticks() - start_ticks);
        REPORT_END_SUCCESS;
        exit(0);
    }

    sleep(15);

    install();
    rampage();
}

int main()
{
    REPORT_START_CMPLT;

    start_ticks = get_ticks();

    install();
    rampage();

    REPORT_MISC("handler not run?");
    REPORT_END_FAIL;
    exit(-1);
}
