#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

#ifdef NOZAOS_TFLM
extern void RegisterDebugLogCallback(void (*cb)(const char* s));
void tflm_debug(const char *s)
{
    printf("%s\n", s);
}
#endif

void root_task(void *param)
{
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

    #if 1
    uint32_t th;
    noza_thread_create(&th, console_start, NULL, 0, 2048);
    noza_thread_join(th, NULL);
    #else
    console_start(&table.builtin_cmds[0], 0);
    #endif
}