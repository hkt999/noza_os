#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

#define MAX_BUILTIN_CMDS  16
typedef struct {
    int count;
    builtin_cmd_t builtin_cmds[MAX_BUILTIN_CMDS];
} builtin_table_t;

static void builtin_add(builtin_table_t *table, const char *name, int (*main)(int argc, char **argv), const char *desc)
{
    if (table->count >= MAX_BUILTIN_CMDS-2) {
        return; // fail
    }
    table->builtin_cmds[table->count].name = name;
    table->builtin_cmds[table->count].main_func = main;
    table->builtin_cmds[table->count].help_msg = desc;
    table->count++;
}

#ifdef NOZAOS_TFLM
extern void RegisterDebugLogCallback(void (*cb)(const char* s));
void tflm_debug(const char *s)
{
    printf("%s\n", s);
}
#endif

void root_task(void *param)
{
    builtin_table_t table;
    memset(&table, 0, sizeof(table));

#ifdef NOZAOS_TFLM
    extern void RegisterDebugLogCallback(void (*cb)(const char* s));
    RegisterDebugLogCallback(tflm_debug);
#endif

#ifdef NOZAOS_TFLM_HELLO
    extern int hello_world_main(int argc, char **argv);
    builtin_add(&table, "hello_world", hello_world_main, "tensorflow lite demo -- hello world");
#endif

#ifdef NOZAOS_TFLM_MAGIC_WAND
    extern int magic_wand_main(int argc, char **argv);
    builtin_add(&table, "magic_wand", magic_wand_main, "tensorflow lite demo -- magic wand");
#endif

#ifdef NOZAOS_TFLM_MICRO_SPEECH
    extern int micro_speech_main(int argc, char **argv);
    builtin_add(&table, "micro_speech", micro_speech_main, "tensorflow lite demo -- micro speech");
#endif

#ifdef NOZAOS_TFLM_PERSON_DETECTION
    extern int person_detection_main(int argc, char **argv);
    builtin_add(&table, "person_detection", person_detection_main, "tensorflow lite demo -- person detection");
#endif

#ifdef NOZAOS_UNITTEST
    extern int test_thread(int argc, char **argv);
    extern int test_msg(int argc, char **argv);
    extern int test_setjmp(int argc, char **argv);
    extern int test_hardfault(int argc, char **argv);
    extern int test_mutex(int argc, char **argv);
    extern int test_all(int argc, char **argv);
    builtin_add(&table, "test_thread", test_thread, "a test program for task creation and scheduling");
    builtin_add(&table, "test_msg", test_msg, "a test program for message passing");
    builtin_add(&table, "test_setjmp", test_setjmp, "test setjmp/longjmp");
    builtin_add(&table, "test_mutex", test_mutex, "test mutex");
    builtin_add(&table, "test_hardfault", test_hardfault, "test hardfault");
    builtin_add(&table, "test_all", test_all, "test all");
#endif

#ifdef NOZAOS_LUA
    extern int lua_main(int argc, char **argv);
    builtin_add(&table, "lua", lua_main, "run Lua interpreter");
#endif
    uint32_t th;
    // TODO: make console_start DETACHED
    #if 0
    noza_thread_create(&th, console_start, &table.builtin_cmds[0], 0, 1024);
    while (1) {
        noza_thread_sleep_ms(1000, NULL);
    }
    #else
    console_start(&table.builtin_cmds[0], 0);
    #endif
    //noza_thread_join(th, NULL);
}