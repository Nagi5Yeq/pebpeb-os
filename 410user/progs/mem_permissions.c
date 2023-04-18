/** @file 410user/progs/mem_permissions.c
 *  @author mpa
 *  @brief Tests memory permissions.
 *  @public yes
 *  @for p3
 *  @covers vm
 *  @status done
 */

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include "410_tests.h"

DEF_TEST_NAME("mem_permissions:");

/* forks off a task to die writing to the address */
void attempt_write(int *addr)
{
  int pid, pid2, status = 333;
  REPORT_LOCAL_INIT;

  REPORT_FAILOUT_ON_ERR((pid = fork()));

  if (pid == 0) {
    *addr = 42;

    REPORT_END_FAIL;
    exit(42);
  }

  REPORT_FAILOUT_ON_ERR((pid2 = wait(&status)));

  if (pid != pid2) {
    report_fmt("Bad pid %d (should be %d)", pid2, pid);
    REPORT_END_FAIL;
    exit(42);
  }
  if (status >= 0) {
    report_fmt("Bad status %d", status);
    REPORT_END_FAIL;
    exit(42);
  }
  report_fmt("Good: attempt_write(0x%x)", addr);
}

/* forks off a task to die reading from the address */
void attempt_read(int *addr)
{
  int pid, pid2, status = 444;
  REPORT_LOCAL_INIT;

  REPORT_FAILOUT_ON_ERR((pid = fork()));

  if (pid == 0) {
    pid = *addr;
    
    REPORT_END_FAIL;
    exit(pid);
  }

  REPORT_FAILOUT_ON_ERR((pid2 = wait(&status)));

  if (pid != pid2) {
    report_fmt("Bad pid %d (should be %d)", pid2, pid);
    REPORT_END_FAIL;
    exit(42);
  }
  if (status >= 0) {
    report_fmt("Bad status %d", status);
    REPORT_END_FAIL;
    exit(42);
  }
  report_fmt("Good: attempt_read(0x%x)", addr);
}

const char header[4096] = "push rodata onto its own page";
const char array[] = "constant";
const char trailer[4096] = "push other things off of rodata page";

int main()
{
  REPORT_START_CMPLT;

  /* try reading/writing kernel memory */
  attempt_read((int *)0x500000);
  attempt_write((int *)0x500000);
  
  /* try writing rodata section */
  attempt_write((int *)array);

  /* try writing text secion */
  attempt_write((int *)main);

  REPORT_END_SUCCESS;

  exit(0);
}
