# Noza OS
Noza OS is a tiny operating system for Rasperberry pico (RP2040 processor) written in C. It provides a minimal set of system calls for creating and managing threads, communication between threads, and thread synchronization.

|ID|Name|Description
| :-----| :---- | :---- |
|NSC_YIELD (0)|thread_yield()|Relinquishes the CPU and allows another thread to run.
NSC_SLEEP (1)|uleep(int us)|Blocks the current thread for the specified number of timer ticks.
NSC_CREATE_THREAD (2)|thread_create(void (*entry)(void *))|Creates a new thread that starts running the specified function.
NSC_TERMINATE_THREAD (3)|terminate_thread()|Terminates the current thread.
NSC_SEND (4)|send(int dest_tid, void *msg, int msglen)|Sends a message to the specified thread.
NSC_RECV (5)|recv(int *src_tid, void *msg, int msglen)|Receives a message from any thread.
NSC_REPLY (6)|reply(int tid, void *reply, int replylen)|Sends a reply message to the specified thread.
NSC_CALL (7)|call(int tid, void *msg, int msglen, void *reply, int replylen)|Sends a message and waits for a reply from the specified thread.
NSC_NBSEND (8)|nbsend(int dest_tid, void *msg, int msglen)|Non-blocking version of send().
NSC_NBRECV (9)|nbrecv(int *src_tid, void *msg, int msglen)|Non-blocking version of recv().
