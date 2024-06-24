// file3.c
#include <stdio.h>
#include "shared.h"
#include "shared1.h"

void print_shared_variable_from_file3() {
    printf("Shared variable from file3: %d\n", shared_variable);
}
