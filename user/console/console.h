#pragma once

typedef void (*callback_t)(int argc, char **argv, void *user_data);

int console_start();
int console_stop();
int console_register(const char *name, callback_t func);
int console_unregister(const char *name);
