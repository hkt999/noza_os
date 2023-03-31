#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"

#include "user/console/noza_console.h"

#ifdef NOZAOS_TFCMSIS_DEMO_H
extern int hello_world_main(int argc, char **argv);
extern int magic_wand_main(int argc, char **argv);
extern int micro_speech_main(int argc, char **argv);
extern int person_detection_main(int argc, char **argv);
builtin_cmd_t builtin_cmds[] = {
    {"hello_world", hello_world_main, "tensorflow lite demo -- hello world"},
    {"magic_wand", magic_wand_main, "tensorflow lite demo -- magic wand"},
    {"micro_speech", micro_speech_main, "tensorflow lite demo -- micro speech"},
     {"person_detection", person_detection_main, "tensorflow lite demo -- person detection"},
    {NULL, NULL}
};

extern void RegisterDebugLogCallback(void (*cb)(const char* s));
void tflm_debug(const char *s)
{
    printf("%s\n", s);
}
#endif

#ifdef NOZAOS_UNITTEST
extern int task_test(int argc, char **argv);
extern int message_test(int argc, char **argv);
extern int thread_join_test(int argc, char **argv);
builtin_cmd_t builtin_cmds[] = {
    {"test_task", task_test, "a demo program for task creation and scheduling"},
    {"test_msg", message_test, "a demo program for message passing"},
    {"test_join", thread_join_test, "a demo program for thread synchronization join"},
    {NULL, NULL}
};
#endif

#ifdef NOZAOS_LUA
extern int lua_main(int argc, char **argv);
builtin_cmd_t builtin_cmds[] = {
    {"test_task", task_demo, "a demo program for task creation and scheduling"},
    {"test_msg", message_demo, "a demo program for message passing"},
    {"test_join", thread_join_demo, "a demo program for thread synchronization join"},
    {"lua", lua_main, "lua interpreter"},
    {NULL, NULL}
};
#endif


void __user_start()
{
    // RegisterDebugLogCallback(tflm_debug);
    uint32_t th = noza_thread_create(console_start, &builtin_cmds[0], 0);

    noza_thread_join(th);
    noza_thread_terminate();
}
