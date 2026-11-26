#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "x86/lifted.h"
#include "win32/process.h"
#include "win32/win32.h"
#include "win32/msvcr100.h"
#include "runtime/log.h"

namespace recomp {
    extern const LiftedFn kLiftedFunctions[];
    extern const uint32_t kLiftedFunctionCount;
    extern uint8_t* g_guest_base;
    void RegisterLiftedFunctions(const LiftedFn* fns, uint32_t count);

    void PumpHostEvents() {
        // Desktop window event pump placeholder
    }
}

int main(int argc, char** argv) {
    std::printf("=====================================================\n");
    std::printf(" Outlast Native Desktop Recompilation Host (Phase 3)\n");
    std::printf("=====================================================\n");
    
    // Explicitly register the lifted function table to guarantee dispatch linking
    recomp::win32::LinkMsvcr100();
    recomp::RegisterLiftedFunctions(recomp::kLiftedFunctions, recomp::kLiftedFunctionCount);
    std::printf("[*] Registered Lifted Functions: %u\n", recomp::kLiftedFunctionCount);
    
    std::string exe_path = "/home/ang/outlast-game/Binaries/Win64/OLGame.exe";
    std::string game_dir = "/home/ang/outlast-game";
    if (argc > 1) {
        exe_path = argv[1];
        // If passed an explicit exe path, derive game_dir from it if possible
        size_t pos = exe_path.rfind("/Binaries/Win64");
        if (pos != std::string::npos) {
            game_dir = exe_path.substr(0, pos);
        } else {
            size_t slash = exe_path.find_last_of("/\\");
            if (slash != std::string::npos) game_dir = exe_path.substr(0, slash);
        }
    }

    recomp::win32::GameConfig config;
    config.guest_dir = "C:\\Games\\Outlast";
    config.exe_name = "Binaries\\Win64\\OLGame.exe";
    config.id = "outlast";
    config.lifted_dlls = {
        {"Binaries\\Win64\\PhysXLoader64.dll", 0x180000000ULL},
        {"Binaries\\Win64\\PhysXCore64.dll", 0x181000000ULL},
        {"Binaries\\Win64\\PhysXCooking64.dll", 0x182000000ULL},
        {"Binaries\\Win64\\PhysXExtensions64.dll", 0x183000000ULL},
        {"Binaries\\Win64\\PhysXDevice64.dll", 0x184000000ULL},
    };

    std::printf("[*] Starting Win64 Subsystem for Outlast...\n");
    std::printf("[*] Binary Target: %s\n", exe_path.c_str());
    
    return recomp::RunProcess(config, exe_path, game_dir, "");
}
