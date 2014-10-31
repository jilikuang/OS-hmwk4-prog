#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>

#define	N_TEST_TASK	125
#define N_TEST_LOOP	100

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
	long ret;
	struct sched_param param;

	pid = fork();

	if (pid == 0) {
		srand (time(NULL));

		for (i = 0; i != N_TEST_LOOP; i++) {
			if (rand() % 2 == 1) {
				if (sched_getscheduler(getpid()) == 6) {
					param.sched_priority = 0;
					ret = sched_setscheduler(getpid(), SCHED_OTHER, &param);
					
					if (ret != 0)
						printf ("set err for %d: %s\n", getpid(), strerror(errno));
				} else {
					param.sched_priority = 50;
					ret = sched_setscheduler(getpid(), 6, &param);
					
					if (ret != 0)
						printf ("set err for %d: %s\n", getpid(), strerror(errno));
				}
			}
		
			test_print(i);
			usleep(rand() % 100 * 10000 + 1000);
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
