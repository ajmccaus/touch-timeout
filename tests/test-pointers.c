#include <stdio.h>

typedef void (*log_func_t)(int, const char *, ...);

int main(void) {
    printf("Size of function pointer: %zu bytes\n", sizeof(log_func_t));
    printf("Size of log_null: %zu bytes\n", sizeof(void*));
    return 0;
}
