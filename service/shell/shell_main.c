#include <stdio.h>
#include <string.h>
#include "nozaos.h"
#include "noza_console_api.h"
#include "service/name_lookup/name_lookup_client.h"
#include "service/console/console_io_client.h"
#include "noza_fs.h"

#define SHELL_PROMPT "noza> "
#define SHELL_BANNER "\nNOZA OS v0.01\n"

static int shell_main(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;

    uint32_t shell_id = 0;
    int reg_ret = name_lookup_register("shell", &shell_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printf("shell: name register failed (%d)\n", reg_ret);
    }

    console_write(SHELL_BANNER, strlen(SHELL_BANNER));
    char line[128];
    for (;;) {
        console_write(SHELL_PROMPT, strlen(SHELL_PROMPT));
        uint32_t len = 0;
        int ret = console_readline(line, sizeof(line), &len);
        if (ret != 0) {
            console_write("console offline\n", 16);
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
            int dir = noza_opendir(path);
            if (dir < 0) {
                console_write("ls: cannot open\n", 17);
            } else {
                noza_fs_dirent_t ent;
                int at_end = 0;
                while (noza_readdir(dir, &ent, &at_end) == 0 && !at_end) {
                    console_write(ent.name, strlen(ent.name));
                    console_write("\n", 1);
                }
                noza_closedir(dir);
            }
        } else if (strcmp(argv[0], "cat") == 0 && argv[1]) {
            int fd = noza_open(argv[1], 0, 0);
            if (fd < 0) {
                console_write("cat: cannot open\n", 18);
            } else {
                char buf[128];
                int r;
                while ((r = noza_read(fd, buf, sizeof(buf))) > 0) {
                    console_write(buf, r);
                }
                noza_close(fd);
            }
        } else if (strcmp(argv[0], "mkdir") == 0 && argv[1]) {
            if (noza_mkdir(argv[1], 0755) != 0) {
                console_write("mkdir failed\n", 13);
            }
        } else if (strcmp(argv[0], "rm") == 0 && argv[1]) {
            if (noza_unlink(argv[1]) != 0) {
                console_write("rm failed\n", 10);
            }
        } else if (strcmp(argv[0], "pwd") == 0) {
            char buf[NOZA_FS_MAX_PATH];
            if (noza_getcwd(buf, sizeof(buf))) {
                console_write(buf, strlen(buf));
                console_write("\n", 1);
            }
        } else if (strcmp(argv[0], "cd") == 0 && argv[1]) {
            if (noza_chdir(argv[1]) != 0) {
                console_write("cd failed\n", 10);
            }
        } else if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "list") == 0) {
            console_write(
                "commands: ls [path], cat <file>, mkdir <path>, rm <path>, cd <path>, pwd\n",
                76);
        } else {
            console_write("command not found: ", 20);
            console_write(argv[0], strlen(argv[0]));
            console_write("\n", 1);
        }
    }
    return 0;
}

static uint8_t shell_stack[2048];
void __attribute__((constructor(120))) shell_init(void *param, uint32_t pid)
{
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(shell_main, shell_stack, sizeof(shell_stack));
}
