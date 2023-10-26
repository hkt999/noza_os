#pragma once

#define NOZA_OS_STACK_SIZE       256         // size of our user task stacks in words
#define NOZA_OS_TASK_LIMIT       32          // number of user task
#define NOZA_OS_TIME_SLICE       10000       // scheduler timer, in us
#define NOZA_OS_PRIORITY_LIMIT   8           // levels of priority
#define NOZA_OS_NUM_CORES        2           // number of cores

#define NOZA_THREAD_DEFAULT_STACK_SIZE	1024	// default stack size
