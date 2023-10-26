#pragma once

#if defined(PTHREAD_CREATE_JOINABLE)
#undef PTHREAD_CREATE_JOINABLE
#endif

#if defined(PTHREAD_CREATE_DETACHED)
#undef PTHREAD_CREATE_DETACHED
#endif

#if defined(SCHED_RR)
#undef SCHED_RR
#endif

#if defined(SCHED_FIFO)
#undef SCHED_FIFO
#endif

#if defined(SCHED_OTHER)
#undef SCHED_OTHER
#endif

#if defined(PTHREAD_INHERIT_SCHED)
#undef PTHREAD_INHERIT_SCHED
#endif

#if defined(PTHREAD_EXPLICIT_SCHED)
#undef PTHREAD_EXPLICIT_SCHED
#endif

#if defined(PTHREAD_SCOPE_SYSTEM)
#undef PTHREAD_SCOPE_SYSTEM
#endif

#if defined(PTHREAD_SCOPE_PROCESS)
#undef PTHREAD_SCOPE_PROCESS
#endif

#if defined(PTHREAD_PROCESS_PRIVATE)
#undef PTHREAD_PROCESS_PRIVATE
#endif

#if defined(PTHREAD_PROCESS_SHARED)
#undef PTHREAD_PROCESS_SHARED
#endif

#if defined(PTHREAD_MUTEX_DEFAULT)
#undef PTHREAD_MUTEX_DEFAULT
#endif

#if defined(PTHREAD_MUTEX_NORMAL)
#undef PTHREAD_MUTEX_NORMAL
#endif

#if defined(PTHREAD_MUTEX_ERRORCHECK)
#undef PTHREAD_MUTEX_ERRORCHECK
#endif

#if defined(PTHREAD_MUTEX_RECURSIVE)
#undef PTHREAD_MUTEX_RECURSIVE
#endif

#if defined(NUM_MUTEX_TYPE)
#undef NUM_MUTEX_TYPE
#endif

#define PTHREAD_CREATE_JOINABLE                 NZ_PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_DETACHED                 NZ_PTHREAD_CREATE_DETACHED

#define SCHED_RR                                NZ_SCHED_RR
#define SCHED_FIFO                              NZ_SCHED_FIFO
#define SCHED_OTHER                             NZ_SCHED_OTHER

#define PTHREAD_INHERIT_SCHED                   NZ_PTHREAD_INHERIT_SCHED
#define PTHREAD_EXPLICIT_SCHED                  NZ_PTHREAD_EXPLICIT_SCHED 

#define PTHREAD_SCOPE_SYSTEM                    NZ_PTHREAD_SCOPE_SYSTEM
#define PTHREAD_SCOPE_PROCESS                   NZ_PTHREAD_SCOPE_PROCESS
#define PTHREAD_PROCESS_PRIVATE                 NZ_PTHREAD_PROCESS_PRIVATE
#define PTHREAD_PROCESS_SHARED                  NZ_PTHREAD_PROCESS_SHARED


#define PTHREAD_MUTEX_DEFAULT                   NZ_PTHREAD_MUTEX_DEFAULT
#define PTHREAD_MUTEX_NORMAL                    NZ_PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_ERRORCHECK                NZ_PTHREAD_MUTEX_ERRORCHECK
#define PTHREAD_MUTEX_RECURSIVE                 NZ_PTHREAD_MUTEX_RECURSIVE
#define NUM_MUTEX_TYPE                          NZ_NUM_MUTEX_TYPE





#define PTHREAD_CREATE_JOINABLE                 NZ_PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_DETACHED                 NZ_PTHREAD_CREATE_DETACHED

#define SCHED_RR                                NZ_SCHED_RR
#define SCHED_FIFO                              NZ_SCHED_FIFO
#define SCHED_OTHER                             NZ_SCHED_OTHER

#define PTHREAD_INHERIT_SCHED                   NZ_PTHREAD_INHERIT_SCHED
#define PTHREAD_EXPLICIT_SCHED                  NZ_PTHREAD_EXPLICIT_SCHED 

#define PTHREAD_SCOPE_SYSTEM                    NZ_PTHREAD_SCOPE_SYSTEM
#define PTHREAD_SCOPE_PROCESS                   NZ_PTHREAD_SCOPE_PROCESS
#define PTHREAD_PROCESS_PRIVATE                 NZ_PTHREAD_PROCESS_PRIVATE
#define PTHREAD_PROCESS_SHARED                  NZ_PTHREAD_PROCESS_SHARED

#define PTHREAD_MUTEX_DEFAULT                   NZ_PTHREAD_MUTEX_DEFAULT
#define PTHREAD_MUTEX_NORMAL                    NZ_PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_ERRORCHECK                NZ_PTHREAD_MUTEX_ERRORCHECK
#define PTHREAD_MUTEX_RECURSIVE                 NZ_PTHREAD_MUTEX_RECURSIVE
#define NUM_MUTEX_TYPE                          NZ_NUM_MUTEX_TYPE

#define pthread_mutex_t                         nz_pthread_mutex_t
#define sched_param                             nz_sched_param
#define pthread_mutexattr_t                     nz_pthread_mutexattr_t
#define pthread_t                               nz_pthread_t

// time wrapper
#define timespec                                nz_timespec
#define nanosleep(p1, p2)                       nz_nanosleep(p1, p2)

#define pthread_create(p1, p2, p3, p4)          nz_pthread_create(p1, p2, p3, p4)
#define pthread_join(p1, p2)                    nz_pthread_join(p1, p2)
#define pthread_exit(p1)                        nz_pthread_exit(p1)
#define pthread_detach(p1)                      nz_pthread_detach(p1)
#define pthread_yield()                         nz_pthread_yield()
#define pthread_cancel(p1)                      nz_pthread_cancel(p1)
#define pthread_self()                          nz_pthread_self()
#define pthread_kill(p1, p2)                    nz_pthread_kill(p1, p2)
#define pthread_equal(p1, p2)                   nz_pthread_equal(p1, p2)
#define pthread_attr_init(p1)                   nz_pthread_attr_init(p1)
#define pthread_attr_destroy(p1)                nz_pthread_attr_destroy(p1)
#define pthread_attr_setdetachstate(p1, p2)     nz_pthread_attr_setdetachstate(p1, p2)
#define pthread_attr_getdetachstate(p1, p2)     nz_pthread_attr_getdetachstate(p1, p2)
#define pthread_attr_setstacksize(p1, p2)       nz_pthread_attr_setstacksize(p1, p2)
#define pthread_attr_getstacksize(p1, p2)       nz_pthread_attr_getstacksize(p1, p2)
#define pthread_attr_setstackaddr(p1, p2)       nz_pthread_attr_setstackaddr(p1, p2)
#define pthread_attr_getstackaddr(p1, p2)       nz_pthread_attr_getstackaddr(p1, p2)
#define pthread_attr_setstack(p1, p2, p3)       nz_pthread_attr_setstack(p1, p2, p3)
#define pthread_attr_getstack(p1, p2, p3)       nz_pthread_attr_getstack(p1, p2, p3)
#define pthread_attr_setguardsize(p1, p2)       nz_pthread_attr_setguardsize(p1, p2)
#define pthread_attr_getguardsize(p1, p2)       nz_pthread_attr_getguardsize(p1, p2)
#define pthread_attr_setschedparam(p1, p2)      nz_pthread_attr_setschedparam(p1, p2)
#define pthread_attr_getschedparam(p1, p2)      nz_pthread_attr_getschedparam(p1, p2)
#define pthread_attr_setschedpolicy(p1, p2)     nz_pthread_attr_setschedpolicy(p1, p2)
#define pthread_attr_getschedpolicy(p1, p2)     nz_pthread_attr_getschedpolicy(p1, p2)
#define pthread_attr_setinheritsched(p1, p2)    nz_pthread_attr_setinheritsched(p1, p2)
#define pthread_attr_getinheritsched(p1, p2)    nz_pthread_attr_getinheritsched(p1, p2)
#define pthread_attr_setscope(p1, p2)           nz_pthread_attr_setscope(p1, p2)
#define pthread_attr_getscope(p1, p2)           nz_pthread_attr_getscope(p1, p2)
#define pthread_mutex_init(p1, p2)              nz_pthread_mutex_init(p1, p2)
#define pthread_mutex_destroy(p1)               nz_pthread_mutex_destroy(p1)
#define pthread_mutex_lock(p1)                  nz_pthread_mutex_lock(p1)
#define pthread_mutex_trylock(p1)               nz_pthread_mutex_trylock(p1)
#define pthread_mutex_unlock(p1)                nz_pthread_mutex_unlock(p1)

#define pthread_attr_t                          nz_pthread_attr_t
#define pthread_mutexattr_init(p1)              nz_pthread_mutexattr_init(p1)
#define pthread_mutexattr_destroy(p1)           nz_pthread_mutexattr_destroy(p1)
#define pthread_mutexattr_getpshared(p1, p2)    nz_pthread_mutexattr_getpshared(p1, p2)
#define pthread_mutexattr_setpshared(p1, p2)    nz_pthread_mutexattr_setpshared(p1, p2)
#define pthread_mutexattr_gettype(p1, p2)       nz_pthread_mutexattr_gettype(p1, p2)
#define pthread_mutexattr_settype(p1, p2)       nz_pthread_mutexattr_settype(p1, p2)

#define pthread_cond_t                          nz_pthread_cond_t
#define pthread_condattr_t                      nz_pthread_condattr_t
#define pthread_cond_init(p1, p2)               nz_pthread_cond_init(p1, p2)
#define pthread_cond_destroy(p1)                nz_pthread_cond_destroy(p1)
#define pthread_cond_wait(p1, p2)               nz_pthread_cond_wait(p1, p2)
#define pthread_cond_timedwait(p1, p2, p3)      nz_pthread_cond_timedwait(p1, p2, p3)
#define pthread_cond_signal(p1)                 nz_pthread_cond_signal(p1)
#define pthread_cond_broadcast(p1)              nz_pthread_cond_broadcast(p1)
#define pthread_condattr_init(p1)               nz_pthread_condattr_init(p1)
#define pthread_condattr_destroy(p1)            nz_pthread_condattr_destroy(p1)
#define pthread_condattr_getpshared(p1, p2)     nz_pthread_condattr_getpshared(p1, p2)
#define pthread_condattr_setpshared(p1, p2)     nz_pthread_condattr_setpshared(p1, p2)
#define pthread_condattr_setclock(p1, p2)       nz_pthread_condattr_setclock(p1, p2)
#define pthread_condattr_getclock(p1, p2)       nz_pthread_condattr_getclock(p1, p2)
#define pthread_atfork(p1, p2, p3)              nz_pthread_atfork(p1, p2, p3)

