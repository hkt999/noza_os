#pragma once

#define NOZA_OS_STACK_SIZE       256         // size of our user task stacks in words
#define NOZA_OS_TASK_LIMIT       32          // number of user task
#define NOZA_OS_THREAD_PSP       0xFFFFFFFD  // exception return behavior (thread mode)
#define NOZA_OS_TIME_SLICE       100000       // scheduler timer, in us
#define NOZA_OS_PRIORITY_LIMIT   4           // levels of priority
#define NOZA_OS_NUM_CORES        1           // number of cores
