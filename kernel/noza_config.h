#pragma once

#define NOZA_OS_ENABLE_IRQ        1          // staged IRQ delivery; flip to 1 when irq service is ready
#define NOZA_OS_STACK_SIZE       192         // size of our user task stacks in words
#define NOZA_OS_TASK_LIMIT       32          // number of user task
#define NOZA_OS_TIME_SLICE       10000       // scheduler timer, in us
#define NOZA_OS_PRIORITY_LIMIT   8           // levels of priority
#define NOZA_OS_NUM_CORES        2           // number of cores
#define NOZA_MAX_SERVICES        8           // number of services
#define NOZA_MAX_PROCESSES      16           // number of processes
#define NOZA_PROC_THREAD_COUNT  32           // number of threads per process
#define NOZA_PROCESS_ENV_SIZE	256          // size of process environment buffer

#define NOZA_ROOT_STACK_SIZE            2048
#define NOZA_THREAD_DEFAULT_STACK_SIZE	1024	// default stack size
#define NOZA_PROCESS_HEAP_SIZE          4096
