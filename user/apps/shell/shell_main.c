#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include "kernel/platform_config.h"
#include "noza_fs.h"
#include "nozaos.h"
#include "app_launcher.h"
#include "platform.h"
#include "noza_console_api.h"
#include "drivers/uart/uart_io_client.h"
#include "service/memory/mem_client.h"
#include "posix/errno.h"
#include "posix/bits/signum.h"
#include "posix/spawn.h"
#include "printk.h"

#define SHELL_BANNER "\nNOZA OS v0.01\n"
#define SHELL_PROMPT_FMT "noza(%u)> "

static int shell_tty_fd = -1;
static uint32_t shell_prompt_counter = 0;
int shell_main(int argc, char **argv);
static int spin_main(int argc, char **argv);
static int exit42_main(int argc, char **argv);

static int spin_main(int argc, char **argv)
{
    volatile uint32_t spin = (uint32_t)argc;
    (void)argv;
    for (;;) {
        spin++;
        __asm__ volatile("" : "+r"(spin));
    }
    return 0;
}

static int exit42_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 42;
}

static void shell_register_apps(void)
{
    static int registered = 0;
    static int failed = 0;
    static const struct {
        const char *path;
        main_t entry;
        uint32_t stack_size;
    } apps[] = {
        {"/sbin/shell", shell_main, 2048},
        {"/sbin/spin", spin_main, 1024},
        {"/sbin/exit42", exit42_main, 1024},
    };

    if (registered || failed) {
        return;
    }

    for (uint32_t app = 0; app < (sizeof(apps) / sizeof(apps[0])); app++) {
        int rc = ESRCH;
        for (int attempt = 0; attempt < 50; attempt++) {
            rc = app_launcher_register(apps[app].path, apps[app].entry, apps[app].stack_size);
            if (rc == 0) {
                break;
            }
            if (rc != ESRCH) {
                printk("shell: app register failed %s (%d)\n", apps[app].path, rc);
                failed = 1;
                return;
            }
            noza_thread_sleep_ms(10, NULL);
        }
        if (rc != 0) {
            failed = 1;
            return;
        }
    }
    registered = 1;
}

static int shell_open_tty(void)
{
    if (shell_tty_fd >= 0) {
        return 0;
    }
    int fd = open("/dev/ttyS0", O_RDWR, 0666);
    if (fd < 0) {
        return -1;
    }
    shell_tty_fd = fd;
    return 0;
}

static void app_write(const char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }
    if (shell_open_tty() == 0) {
        size_t offset = 0;
        while (offset < len) {
            int w = write(shell_tty_fd, buf + offset, (uint32_t)(len - offset));
            if (w <= 0) {
                shell_tty_fd = -1;
                break; // error, fall through to error notice
            }
            offset += (size_t)w;
        }
        if (offset == len) {
            return;
        }
    }
    console_write("error on write, fallback\n", 26);
}

static void app_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1;
    app_write(buf, len);
}

static void shell_print_identity(void)
{
    uint32_t pid = 0;
    if (noza_thread_self(&pid) != 0) {
        extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];
        uint32_t core = platform_get_running_core();
        if (core < NOZA_OS_NUM_CORES) {
            pid = NOZAOS_PID[core];
        }
    }
    if (pid != 0) {
        app_printf("shell pid=%u\n", pid);
    }
}

static int shell_tokenize(char *line, char *argv[], int max_argc)
{
    int argc = 0;
    char *cursor = line;
    while (*cursor) {
        while (*cursor && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        if (argc >= max_argc) {
            return -E2BIG;
        }
        argv[argc++] = cursor;
        while (*cursor && !isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        *cursor++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

static int shell_resolve_command_path(const char *file, char path[NOZA_FS_MAX_PATH])
{
    if (file == NULL || path == NULL) {
        return EINVAL;
    }
    if (strchr(file, '/') != NULL) {
        size_t len = strnlen(file, NOZA_FS_MAX_PATH);
        if (len == 0 || len >= NOZA_FS_MAX_PATH) {
            return ENAMETOOLONG;
        }
        memcpy(path, file, len);
        path[len] = '\0';
        return 0;
    }

    static const char prefix[] = "/sbin/";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t name_len = strnlen(file, NOZA_FS_MAX_PATH);
    if (name_len == 0 || prefix_len + name_len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }
    memcpy(path, prefix, prefix_len);
    memcpy(path + prefix_len, file, name_len);
    path[prefix_len + name_len] = '\0';
    return 0;
}

static void shell_spawn_command(char *argv[])
{
    pid_t child = 0;
    int rc = posix_spawnp(&child, argv[0], NULL, NULL, argv, NULL);
    if (rc != 0) {
        app_printf("%s: spawn failed (%d)\n", argv[0], rc);
        return;
    }
    app_printf("spawned pid %u\n", (unsigned)child);
}

static void shell_exec_command(char *argv[], int argc)
{
    if (argc < 2) {
        app_printf("usage: exec <app> [args]\n");
        return;
    }

    char path[NOZA_FS_MAX_PATH];
    int rc = shell_resolve_command_path(argv[1], path);
    if (rc != 0) {
        app_printf("exec: bad path (%d)\n", rc);
        return;
    }

    if (execve(path, &argv[1], NULL) != 0) {
        app_printf("exec: failed (%d)\n", noza_get_errno());
    }
}

static const char *shell_proc_state_name(uint32_t state)
{
    switch (state) {
    case APP_LAUNCHER_PROC_STATE_RUNNING:
        return "RUN";
    case APP_LAUNCHER_PROC_STATE_STOPPED:
        return "STOP";
    case APP_LAUNCHER_PROC_STATE_EXITED:
        return "EXIT";
    default:
        return "UNK";
    }
}

static int shell_parse_u32(const char *text, uint32_t *value)
{
    uint32_t result = 0;

    if (text == NULL || *text == '\0' || value == NULL) {
        return EINVAL;
    }
    while (*text) {
        if (*text < '0' || *text > '9') {
            return EINVAL;
        }
        result = result * 10u + (uint32_t)(*text - '0');
        text++;
    }
    *value = result;
    return 0;
}

static int shell_parse_signal_arg(const char *text, uint32_t *signum)
{
    if (signum == NULL) {
        return EINVAL;
    }
    if (text == NULL) {
        *signum = SIGTERM;
        return 0;
    }
    if (text[0] != '-') {
        return EINVAL;
    }
    return shell_parse_u32(text + 1, signum);
}

static void shell_kill_command(char *argv[], int argc)
{
    uint32_t signum = SIGTERM;
    uint32_t pid = 0;
    int argi = 1;
    int rc;

    if (argc < 2) {
        app_printf("usage: kill [-signum] <pid>\n");
        return;
    }
    if (argv[argi][0] == '-') {
        rc = shell_parse_signal_arg(argv[argi], &signum);
        if (rc != 0 || signum == 0 || signum > 31) {
            app_printf("kill: bad signal\n");
            return;
        }
        argi++;
    }
    if (argi >= argc || shell_parse_u32(argv[argi], &pid) != 0 || pid == 0) {
        app_printf("kill: bad pid\n");
        return;
    }

    rc = noza_thread_kill(pid, (int)signum);
    if (rc != 0) {
        app_printf("kill: failed (%d)\n", rc);
        return;
    }

    switch (signum) {
    case SIGTERM:
    case SIGKILL:
    case SIGSTOP:
    case SIGTSTP:
    case SIGCONT:
        app_launcher_signal_notify(pid, signum);
        break;
    default:
        break;
    }
}

static void shell_show_processes(void)
{
    app_launcher_msg_t *msg = (app_launcher_msg_t *)noza_malloc(sizeof(app_launcher_msg_t));
    if (msg == NULL) {
        app_printf("ps: no memory\n");
        return;
    }
    app_printf("PID   PPID  THR  STATE CMD\n");

    uint32_t offset = 0;
    for (;;) {
        int rc = app_launcher_list_processes(offset, APP_LAUNCHER_LIST_BATCH, msg);
        if (rc != 0) {
            app_printf("ps: failed (%d)\n", rc);
            break;
        }
        for (uint32_t i = 0; i < msg->proc_list.count; i++) {
            app_launcher_proc_item_t *item = &msg->proc_list.items[i];
            app_printf(
                "%-5u %-5u %-4u %-5s %s\n",
                (unsigned)item->pid,
                (unsigned)item->ppid,
                (unsigned)item->thread_count,
                shell_proc_state_name(item->state),
                item->path[0] ? item->path : "(unknown)");
        }
        if (msg->proc_list.count == 0 || offset + msg->proc_list.count >= msg->proc_list.total) {
            break;
        }
        offset += msg->proc_list.count;
    }
    noza_free(msg);
}

static void shell_wait_command(char *argv[], int argc)
{
    uint32_t pid = 0;
    int status = 0;

    if (argc < 2 || shell_parse_u32(argv[1], &pid) != 0 || pid == 0) {
        app_printf("usage: wait <pid>\n");
        return;
    }

    pid_t rc = waitpid((pid_t)pid, &status, 0);
    if (rc < 0) {
        app_printf("wait: failed (%d)\n", noza_get_errno());
        return;
    }
    if (WIFEXITED(status)) {
        app_printf("wait: pid=%u exit=%d\n", (unsigned)rc, WEXITSTATUS(status));
        return;
    }
    if (WIFSIGNALED(status)) {
        app_printf("wait: pid=%u signal=%d\n", (unsigned)rc, WTERMSIG(status));
        return;
    }
    app_printf("wait: pid=%u status=%d\n", (unsigned)rc, status);
}

int shell_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    shell_tty_fd = -1;
    shell_prompt_counter = 0;
    shell_register_apps();
    app_printf("%s", SHELL_BANNER);
    shell_print_identity();
    char line[128];
    for (;;) {
        app_printf(SHELL_PROMPT_FMT, shell_prompt_counter++);
        uint32_t len = 0;
        int ret = console_readline(line, sizeof(line), &len);
        if (ret != 0) {
            app_printf("console offline\n");
            noza_thread_sleep_ms(500, NULL);
            continue;
        }
        if (len == 0) {
            app_printf("\r\n");
            continue;
        }
        char *args[APP_LAUNCHER_MAX_ARGC + 1] = {0};
        int cmd_argc = shell_tokenize(line, args, APP_LAUNCHER_MAX_ARGC);
        if (cmd_argc < 0) {
            app_printf("too many arguments\n");
            continue;
        }
        if (cmd_argc == 0) {
            continue;
        }
        if (strcmp(args[0], "ls") == 0) {
            const char *path = args[1] ? args[1] : ".";
            DIR *dir = opendir(path);
            if (dir == NULL) {
                app_printf("ls: cannot open\n");
            } else {
                struct dirent *ent;
                while ((ent = readdir(dir)) != NULL) {
                    app_write(ent->d_name, strlen(ent->d_name));
                    app_printf("\n");
                }
                closedir(dir);
            }
        } else if (strcmp(args[0], "cat") == 0 && args[1]) {
            int fd = open(args[1], O_RDONLY, 0);
            if (fd < 0) {
                app_printf("cat: cannot open\n");
            } else {
                char buf[128];
                int r;
                while ((r = read(fd, buf, sizeof(buf))) > 0) {
                    app_write(buf, (size_t)r);
                }
                close(fd);
            }
        } else if (strcmp(args[0], "mkdir") == 0 && args[1]) {
            if (mkdir(args[1], 0755) != 0) {
                app_printf("mkdir failed\n");
            }
        } else if (strcmp(args[0], "rm") == 0 && args[1]) {
            if (unlink(args[1]) != 0) {
                app_printf("rm failed\n");
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            char buf[NOZA_FS_MAX_PATH];
            if (getcwd(buf, sizeof(buf))) {
                app_write(buf, strlen(buf));
                app_printf("\n");
            }
        } else if (strcmp(args[0], "cd") == 0 && args[1]) {
            if (chdir(args[1]) != 0) {
                app_printf("cd failed\n");
            }
        } else if (strcmp(args[0], "pid") == 0) {
            shell_print_identity();
        } else if (strcmp(args[0], "ps") == 0) {
            shell_show_processes();
        } else if (strcmp(args[0], "kill") == 0) {
            shell_kill_command(args, cmd_argc);
        } else if (strcmp(args[0], "wait") == 0) {
            shell_wait_command(args, cmd_argc);
        } else if (strcmp(args[0], "exec") == 0) {
            shell_exec_command(args, cmd_argc);
        } else if (strcmp(args[0], "help") == 0 || strcmp(args[0], "list") == 0) {
            app_printf("commands: ls [path], cat <file>, mkdir <path>, rm <path>, cd <path>, pwd, pid, ps, kill [-signum] <pid>, wait <pid>, exec <app>\n");
            app_printf("unknown commands are resolved via /sbin and spawned with posix_spawnp\n");
        } else {
            shell_spawn_command(args);
        }
    }
    return 0;
}
