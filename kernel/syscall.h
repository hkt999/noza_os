#pragma once

// thread & scheduling
#define NSC_YIELD                       0
#define NSC_SLEEP                       1
#define NSC_KILL                        2
#define NSC_THREAD_CREATE               3
#define NSC_THREAD_CHANGE_PRIORITY      4
#define NSC_THREAD_JOIN                 5
#define NSC_THREAD_DETACH               6
#define NSC_THREAD_TERMINATE            7
#define NSC_THREAD_SELF                 8
// IPC
#define NSC_RECV                        9
#define NSC_REPLY                       10
#define NSC_CALL                        11
#define NSC_NB_RECV                     12
#define NSC_NB_CALL                     13
#define NSC_NUM_SYSCALLS                14

