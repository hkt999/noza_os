extern int hello_world_main(int argc, char **argv);

#include "noza_console.h"
// register unit test function
void __attribute__((constructor(1000))) register_hello_world()
{
    console_add_command("hello_world", hello_world_main, "run tensorflow lite hello_world example");
}