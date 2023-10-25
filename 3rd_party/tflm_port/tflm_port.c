#include <stdio.h>

static void tflm_debug(const char *s)
{
    printf("%s\n", s);
}

void __attribute__((constructor(1000))) tflm_init()
{
    extern void RegisterDebugLogCallback(void (*cb)(const char* s));
	RegisterDebugLogCallback(tflm_debug);
}