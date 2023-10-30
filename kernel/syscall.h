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
#define NSC_NUM_SYSCALLS                12

