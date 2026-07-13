#ifndef T32_LOG_H
#define T32_LOG_H

#include <stdbool.h>

bool t32_log_open(const char *path);
void t32_log_close(void);
void t32_log_printf(const char *format, ...);

#endif
