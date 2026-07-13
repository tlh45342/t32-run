#include "t32_log.h"

#include <stdarg.h>
#include <stdio.h>

static FILE *log_file = NULL;

bool t32_log_open(const char *path)
{
    t32_log_close();

    log_file = fopen(path, "wb");
    return log_file != NULL;
}

void t32_log_close(void)
{
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void t32_log_printf(const char *format, ...)
{
    va_list console_arguments;
    va_list log_arguments;

    va_start(console_arguments, format);
    va_copy(log_arguments, console_arguments);

    vfprintf(stdout, format, console_arguments);
    fflush(stdout);

    if (log_file) {
        vfprintf(log_file, format, log_arguments);
        fflush(log_file);
    }

    va_end(log_arguments);
    va_end(console_arguments);
}
