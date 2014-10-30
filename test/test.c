#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>

static inline void test_print(void)
{
	printf("%d with %d is running\n", getpid(), sched_getscheduler(0));
}

int main(int argc, char **argv)
{
	pid_t pid;
	struct sched_param param = {50};

	if (sched_setscheduler(getpid(), 6, &param) < 0) {
		printf("error: %s\n", strerror(errno));
		return -1;
	}

	pid = fork();
	if (pid == 0) {
		int i = 0;
		for (i = 0; i < 30; i++) {
			test_print();
			sleep(2);
		}
	} else if (pid > 0) {
		while (1) {
			test_print();
			sleep(3);
		}
	} else {
		printf("error: %s\n", strerror(errno));
	}

	return 0;
}
