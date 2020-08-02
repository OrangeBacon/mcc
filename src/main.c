#include <stdlib.h>
#include "driver.h"
#include "memory.h"

// most processing is dispatched in driver.c
// main just runs global program initialisation and the driver
int main(int argc, char** argv) {

    // initialise global memory allocator
    ArenaInit();

    bool hadError = driver(argc, argv);

    return hadError ? EXIT_FAILURE : EXIT_SUCCESS;
}