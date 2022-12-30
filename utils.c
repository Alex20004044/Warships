#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

long to_long(const char *str) {
    char *ptr;
    long result = strtol(str, &ptr, 0);
    if (strlen(str) != ptr - str) {
        fprintf(stderr, "failed to convert str to long: %s\n", str);
        exit(1);
    }
    return result;
}
int max(int a, int b) {
    return a > b ? a : b;
}

int min(int a, int b) {
    return a < b ? a : b;
}
