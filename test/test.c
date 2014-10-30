#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

struct test_param {
	int pri;
};

int main(int argc, char **argv)
{
	struct test_param param = {50};

	if (syscall(__NR_sched_setscheduler, getpid(), 6, &param) < 0) {
		printf("error: %s\n", strerror(errno));
		return -1;
	}

	while (1) {
		printf("test program %d running.\n", getpid());
		sleep(5);
	}

	return 0;
}
