extern int person_detection_main(int argc, char **argv);

#include "noza_console.h"
// register unit test function
void __attribute__((constructor(1000))) register_person_detection()
{
    console_add_command("person_detection", person_detection_main, "run tensorflow lite person_detection example");
}