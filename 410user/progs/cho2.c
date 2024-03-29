/** @file 410user/progs/cho2.c
 *  @author de0u
 *  @author ctokar
 *  @brief "Continuous hours of operation" - run many tests.
 *  @public yes
 *  @for p3
 *  @covers fork exec wait gettid set_status vanish sleep
 *  @todo Progress checking?
 */

#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <simics.h>
#include "410_tests.h"
#include <report.h>

DEF_TEST_NAME("cho2:");

/* Every UPDATE_FREQUENCY reaps, print out an update of 
 * how many children of each type are left.
 */
#define UPDATE_FREQUENCY 50

#define SERIOUSLY_DEAD (0xdeaddead)

struct prog {
	char *name;
	int pid;
	int count;
} progs[] = {
	{"getpid_test1", -1, 13},
	{"yield_desc_mkrun", -1, 100},
	{"remove_pages_test1", -1, 100},
	{"loader_test1", -1, 100},
	{"fork_wait", -1, 100},
	{"swexn_rampage", -1, 1},
	{"mem_permissions", -1, 31},
	{"minclone_mem", -1, 17}
};

int main()
{
	int active_progs = sizeof (progs) / sizeof (progs[0]);
	int active_processes = 0;
	struct prog *p, *fence;
	int reap_count = 0;

	report_start(START_CMPLT);
	TEST_PROG_ENGAGE(300);

	fence = progs + active_progs;

	while ((active_progs > 0) || (active_processes > 0)) {

		/* launch some processes */
		for (p = progs; p < fence; ++p) {
			if ((p->count > 0) && (p->pid == -1)) {
				int pid = fork();

				if (pid < 0) {
					sleep(1);
				} else if (pid == 0) {
					char *args[2];

					report_misc("After fork(): I am a child!");
					TEST_PROG_PROGRESS;

					args[0] = p->name;
					args[1] = 0;
					exec(p->name, args);
					report_misc("exec() failed (missing object?)");
					report_end(END_FAIL);
					exit(SERIOUSLY_DEAD);
				} else {
					p->pid = pid;
					++active_processes;
				}
			}
		}

		/* reap a child */
		if (active_processes > 0) {
			int pid, status;
			pid = wait(&status);
			if (status == SERIOUSLY_DEAD)
			  exit(SERIOUSLY_DEAD);
			report_fmt("wait() done: %d with status %d", pid, status);
			TEST_PROG_PROGRESS;
			++reap_count;
			for (p = progs; p < fence; ++p) {
				if (p->pid == pid) {
					p->pid = -1;
					--p->count;
					if (p->count <= 0)
						--active_progs;
					--active_processes;
				}
				if (reap_count % UPDATE_FREQUENCY == 0) {
					report_fmt("%s: %d left", p->name, p->count);
				}
			}
		}
	}

	report_end(END_SUCCESS);
	exit(0);
}
