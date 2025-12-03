#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include "noza_fs.h"
#include "nozaos.h"
#include "noza_console_api.h"
#include "drivers/uart/uart_io_client.h"
#include "printk.h"

#define SHELL_PROMPT "noza> "
#define SHELL_BANNER "\nNOZA OS v0.01\n"

static int shell_tty_fd = -1;

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

int shell_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    app_printf("%s", SHELL_BANNER);
    char line[128];
    for (;;) {
        app_printf("%s", SHELL_PROMPT);
        uint32_t len = 0;
        int ret = console_readline(line, sizeof(line), &len);
        if (ret != 0) {
            app_printf("console offline\n");
            noza_thread_sleep_ms(500, NULL);
            continue;
        }
        if (len == 0) {
            continue;
        }
        char *argv[4] = {0};
        argv[0] = line;
        char *sp = strchr(line, ' ');
        if (sp) {
            *sp = '\0';
            argv[1] = sp + 1;
        }
        if (strcmp(argv[0], "ls") == 0) {
            const char *path = argv[1] ? argv[1] : ".";
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
        } else if (strcmp(argv[0], "cat") == 0 && argv[1]) {
            int fd = open(argv[1], O_RDONLY, 0);
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
        } else if (strcmp(argv[0], "mkdir") == 0 && argv[1]) {
            if (mkdir(argv[1], 0755) != 0) {
                app_printf("mkdir failed\n");
            }
        } else if (strcmp(argv[0], "rm") == 0 && argv[1]) {
            if (unlink(argv[1]) != 0) {
                app_printf("rm failed\n");
            }
        } else if (strcmp(argv[0], "pwd") == 0) {
            char buf[NOZA_FS_MAX_PATH];
            if (getcwd(buf, sizeof(buf))) {
                app_write(buf, strlen(buf));
                app_printf("\n");
            }
        } else if (strcmp(argv[0], "cd") == 0 && argv[1]) {
            if (chdir(argv[1]) != 0) {
                app_printf("cd failed\n");
            }
        } else if (strcmp(argv[0], "clear") == 0) {
            app_printf("\033[2J\033[H");
        } else if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "list") == 0) {
            app_printf("commands: ls [path], cat <file>, mkdir <path>, rm <path>, cd <path>, pwd, clear\n");
        } else {
            app_printf("command not found: %s\n", argv[0]);
        }
    }
    return 0;
}
