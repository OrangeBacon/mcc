#include <stdlib.h>
#include "driver.h"
#include "memory.h"
#include "file.h"

// most processing is dispatched in driver.c
// main just runs global program initialisation and the driver
int main(int argc, char** argv) {

    // initialise global memory allocator
    ArenaInit();

    // initalise file system wrapper
    FilesInit();

    bool hadError = driver(argc, argv);

    return hadError ? EXIT_FAILURE : EXIT_SUCCESS;
}