#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <stdlib.h>

#define	N_TEST_TASK	10

static int get_cpu_id() {

    unsigned cpu_id;

    if (syscall(__NR_getcpu, &cpu_id, NULL, NULL) < 0) {
        return -1;
    } else {
        return (int) cpu_id;
    }
}

static inline void test_print(int run)
{
	printf(
		"task %d: run %d using %d @ cpu %d\n",
		getpid(),
		run,
		sched_getscheduler(0),
		get_cpu_id());
}

static void create_new_task()
{
	pid_t pid;
	int i;

	pid = fork();

	if (pid == 0) {
		for (i = 0; i < 100; i++) {
			test_print(i);
			sleep(1);
		}

		exit (0);
	} else if (pid < 0) {
		printf ("Fork failed.\n");
	}
}

int main(int argc, char **argv)
{
	int i;
	struct sched_param param = {.sched_priority = 50};

	if (sched_setscheduler(getpid(), 6, &param) < 0) {
		printf("error: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < N_TEST_TASK; ++i) {
		create_new_task();
	}

	return 0;
}
