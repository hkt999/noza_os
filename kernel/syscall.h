#pragma once

enum {
    // thread & scheduling
    NSC_YIELD = 0,
    NSC_SLEEP,
    NSC_CREATE_THREAD,
    NSC_TERMINATE_THREAD,
    // IPC
    NSC_RECV,
    NSC_REPLY,
    NSC_CALL,
    NSC_NB_RECV,
    NSC_NB_CALL,
    NSC_NUM_SYSCALLS,
};

#define NSC_HARDFAULT   255