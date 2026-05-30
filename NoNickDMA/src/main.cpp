#include <thread>
#include "Memory/Memory.h"
#include <iostream>
#include "game/game.h"
#include "window/window.hpp"
#include "globals.h"
#include "makcu/makcu.h" 
#include "makcu/makcu_wrapper.h"

void clearConsole()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}


std::string g_validExecutable;

void InitMemory()
{
    std::vector<std::string> executables = {
        "FiveM_GTAProcess.exe",                
        "FiveM_b2802_GTAProcess.exe",         
        "FiveM_b2944_GTAProcess.exe",
        "FiveM_b3095_GTAProcess.exe",
        "FiveM_b3407_GTAProcess.exe"
    };

    bool initialized = false;

    for (const auto& exe : executables) {

        if (mem.Init(exe.c_str(), true, true)) {
            std::cout << "DMA Initialized with: " << exe << "!!\n";
            g_validExecutable = exe;  // Store globally
            initialized = true;
            break;
        }
        else {

        }
    }

    if (!initialized) {
        std::cout << "Failed to INIT DMA with any process... Is the game running?\n";
        return;
    }

    uint64_t base = mem.GetBaseDaddy(g_validExecutable.c_str());
    std::cout << "Base ::: 0x" << std::hex << base << "\n";
}

int main()
{
    std::cout << "===================================\n";
    std::cout << "             NoNick DMA\n";
    std::cout << "===================================\n\n";

    std::cout << "[*] Initializing DMA..." << std::endl;
    InitMemory();

    if (g_validExecutable.empty()) {
        std::cout << "[!] Failed to initialize DMA. Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    Overlay overlay;
    overlay.SetupOverlay("NoNick DMA");

    FiveM::Setup();

    int build_version = FiveM::GetCurrentBuildVersion();
    if (build_version > 0) {
        std::cout << "[*] FiveM build: b" << build_version << std::endl;
    }
    else {
        std::cout << "[!] Warning: Could not detect build version, using default offsets" << std::endl;
    }

    makcu_wrapper::MakcuInitialize("");

    std::cout << "[*] Press INSERT to open menu\n" << std::endl;

    while (overlay.shouldRun) {
        overlay.StartRender();

        if (overlay.RenderMenu)
            overlay.Render();

        overlay.EndRender();
    }

    std::cout << "[*] Shutting down..." << std::endl;
    return 0;
}