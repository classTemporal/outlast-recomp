#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "x86/lifted.h"

namespace recomp {
    extern const LiftedFn kLiftedFunctions[];
    extern const uint32_t kLiftedFunctionCount;
    extern uint8_t* g_guest_base;
}

int main(int argc, char** argv) {
    std::printf("=====================================================\n");
    std::printf(" Outlast Native Desktop Recompilation Host (Phase 3)\n");
    std::printf("=====================================================\n");
    std::printf("[*] Registered Lifted Functions: %u\n", recomp::kLiftedFunctionCount);
    std::printf("[*] Runtime initialization complete.\n");
    return 0;
}
