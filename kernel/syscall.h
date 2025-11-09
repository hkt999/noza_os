#pragma once

// thread & scheduling
#define NSC_THREAD_SLEEP                0
#define NSC_THREAD_KILL                 1
#define NSC_THREAD_CREATE               2
#define NSC_THREAD_CHANGE_PRIORITY      3
#define NSC_THREAD_JOIN                 4
#define NSC_THREAD_DETACH               5
#define NSC_THREAD_TERMINATE            6
// IPC
#define NSC_RECV                        7
#define NSC_REPLY                       8
#define NSC_CALL                        9
#define NSC_NB_RECV                     10
#define NSC_NB_CALL                     11
#define NSC_FUTEX_WAIT                  12
#define NSC_FUTEX_WAKE                  13
// timer
#define NSC_TIMER_CREATE                14
#define NSC_TIMER_DELETE                15
#define NSC_TIMER_ARM                   16
#define NSC_TIMER_CANCEL                17
#define NSC_TIMER_WAIT                  18

#define NSC_NUM_SYSCALLS                19

// timer flags
#define NOZA_TIMER_FLAG_PERIODIC        0x01
