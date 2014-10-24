/*
 * SYSCALL defined for HW 4
 */

#include <linux/syscalls.h>

SYSCALL_DEFINE2(sched_set_CPUgroup, int, numCPU, int, group)
{
	return 0;
}
