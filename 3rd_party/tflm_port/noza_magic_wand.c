extern int magic_wand_main(int argc, char **argv);

#include "noza_console.h"
// register unit test function
void __attribute__((constructor(1000))) register_magic_wand()
{
    console_add_command("magic_wand", magic_wand_main, "run tensorflow lite magic_wand example");
}