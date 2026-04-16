#include <stdarg.h>
#include <stdio.h>


static char last_error[256];

void gj_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vsnprintf(last_error, sizeof(last_error), fmt, args);

    va_end(args);
}

const char* gj_get_last_error(void) {
    return last_error;
}
