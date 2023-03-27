#pragma once

enum {
    // thread & scheduling
    NSC_YIELD = 0,
    NSC_SLEEP,
    NSC_THREAD_CREATE,
    NSC_THREAD_CHANGE_PRIORITY,
    NSC_THREAD_JOIN,
    NSC_THREAD_TERMINATE,
    // IPC
    NSC_RECV,
    NSC_REPLY,
    NSC_CALL,
    NSC_NB_RECV,
    NSC_NB_CALL,
    NSC_NUM_SYSCALLS,
};

