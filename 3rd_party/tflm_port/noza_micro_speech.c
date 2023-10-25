extern int micro_speech_main(int argc, char **argv);

#include "noza_console.h"
// register unit test function
void __attribute__((constructor(1000))) register_micro_speech()
{
    console_add_command("micro_speech", micro_speech_main, "run tensorflow lite micro_speech example");
}