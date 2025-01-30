#include "kernel.h"
#include <iostream>

int main(int argc, char** argv) {
    // Initialize driver
    pid = getPID("com.pubg.imobile");
    if (pid <= 0) {
        std::cerr << "[-] Failed to get PID!" << std::endl;
        return EXIT_FAILURE;
    }

    printf("PID = %d\n", pid);

    // Get module base address
    uint64_t base = driver->getModuleBase("libUE4.so");
    if (base == 0) {
        std::cerr << "[-] Failed to get module base address!" << std::endl;
        return EXIT_FAILURE;
    }

    printf("Base Address = 0x%lx\n", base);

    // Read values from memory
    uint64_t pointer = driver->read<uint64_t>(base + 0x0);
    printf("Pointer = 0x%lx\n", pointer);

    int intValue = driver->read<int>(base + 0x0);
    printf("Integer Value = %d\n", intValue);

    float floatValue = driver->read<float>(base + 0x0);
    printf("Float Value = %f\n", floatValue);

    // Console output in UTF-8
    std::cout << "666这个入是桂" << std::endl;

    return EXIT_SUCCESS;
}
