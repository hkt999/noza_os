#include <sys/types.h>
#include <sys/wait.h>
#include "app_launcher.h"
#include "kernel/platform_config.h"
#include "nozaos.h"
#include "platform.h"
#include "posix/errno.h"

extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];

pid_t waitpid(pid_t pid, int *status, int options)
{
    if (pid == 0 || pid < -1) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if ((options & ~WNOHANG) != 0) {
        noza_set_errno(ENOSYS);
        return -1;
    }

    uint32_t self_pid = 0;
    if (noza_thread_self(&self_pid) != 0 || self_pid == 0) {
        uint32_t core = platform_get_running_core();
        if (core < NOZA_OS_NUM_CORES) {
            self_pid = NOZAOS_PID[core];
        }
    }
    if (self_pid == 0) {
        noza_set_errno(ESRCH);
        return -1;
    }

    uint32_t reaped_pid = 0;
    int32_t wait_status = 0;
    int rc = app_launcher_wait(self_pid, (int32_t)pid, (uint32_t)options, &reaped_pid, &wait_status);
    if (rc == 0) {
        if (reaped_pid == 0) {
            return 0;
        }
        if (status != NULL) {
            *status = wait_status;
        }
        return (pid_t)reaped_pid;
    }
    if (rc == EAGAIN && (options & WNOHANG) != 0) {
        return 0;
    }
    noza_set_errno(rc);
    return -1;
}
