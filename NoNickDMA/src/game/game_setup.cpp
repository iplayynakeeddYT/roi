#include "game_setup.h"
#include "offsets.h"
#include "../DMALibrary/Memory/Memory.h"
#include "../globals.h"
#include "../playerInfo/PedData.h"
#include <iostream>

// Explicitly declare access to the global cache manager
extern PedCacheManager g_pedCacheManager;

namespace FiveM {
    void Setup() {
        using namespace offset;

        auto game_base = mem.GetBaseDaddy(g_validExecutable);

        // Get build version using date string detection
        buildVersion = GetBuildVersion();

        // Find matching offsets for the detected build
        const BuildOffsets* build_offset = GetOffsetsForBuild(buildVersion);

        if (build_offset) {
            std::cout << "[FiveM] Loading offsets for build " << buildVersion << std::endl;

            // Read world, replay, viewport, camera from their offsets
            world = mem.Read<uintptr_t>(game_base + build_offset->world_offset);
            replay = mem.Read<uintptr_t>(game_base + build_offset->replay_offset);
            viewport = mem.Read<uintptr_t>(game_base + build_offset->viewport_offset);

            if (build_offset->camera_offset != 0) {
                camera = mem.Read<uintptr_t>(game_base + build_offset->camera_offset);
            }

            // LocalPlayer is always at world + 0x8
            localplayer = mem.Read<uintptr_t>(world + 0x8);

            // Set the relative offsets (these are offsets from ped, not from base)
            playerInfo = build_offset->playerInfo_offset;
            playerHealth = build_offset->playerHealth_offset;
            playerPosition = build_offset->playerPosition_offset;
            boneMatrix = build_offset->boneMatrix_offset;

            if (build_offset->boneList_offset != 0) {
                boneList = build_offset->boneList_offset;
            }
        }
        else {
            std::cout << "[FiveM] Warning: Unknown build " << buildVersion << ", using latest known offsets" << std::endl;
            std::cout << "[FiveM] The cheat may not work correctly! Please update offsets for this build." << std::endl;

            // Use the latest known offsets as fallback
            const BuildOffsets* latest = GetOffsetsForBuild(3095); // Latest verified build
            if (!latest) {
                // If even that fails, use hardcoded values from 3095
                world = mem.Read<uintptr_t>(game_base + 0x2593320);
                replay = mem.Read<uintptr_t>(game_base + 0x1F58B58);
                viewport = mem.Read<uintptr_t>(game_base + 0x20019E0);
                camera = mem.Read<uintptr_t>(game_base + 0x20025B8);
                localplayer = mem.Read<uintptr_t>(world + 0x8);
                playerInfo = 0x10A8;
                boneList = 0x48;
            }
            else {
                world = mem.Read<uintptr_t>(game_base + latest->world_offset);
                replay = mem.Read<uintptr_t>(game_base + latest->replay_offset);
                viewport = mem.Read<uintptr_t>(game_base + latest->viewport_offset);

                if (latest->camera_offset != 0) {
                    camera = mem.Read<uintptr_t>(game_base + latest->camera_offset);
                }

                localplayer = mem.Read<uintptr_t>(world + 0x8);
                playerInfo = latest->playerInfo_offset;

                if (latest->boneList_offset != 0) {
                    boneList = latest->boneList_offset;
                }
            }
        }

        // Check if we're using placeholder offsets
        bool using_placeholder = false;
        if (buildVersion >= 3323 && buildVersion <= 3407) {
            // These builds are currently using placeholder offsets
            using_placeholder = true;
        }

        if (using_placeholder) {
            std::cout << "[FiveM] WARNING: Build " << buildVersion << " is using PLACEHOLDER offsets!" << std::endl;
            std::cout << "[FiveM] The cheat may not work correctly until proper offsets are found." << std::endl;
            std::cout << "[FiveM] Check UnknownCheats.me for updated offsets for this build." << std::endl;
        }

        // Validate that we read valid pointers
        if (world == 0 || viewport == 0) {
            std::cout << "[FiveM] Error: Failed to read valid pointers." << std::endl;
            std::cout << "  World: 0x" << std::hex << world << std::endl;
            std::cout << "  Viewport: 0x" << std::hex << viewport << std::endl;
            std::cout << "  Base: 0x" << std::hex << game_base << std::dec << std::endl;
        }
        else {
            std::cout << "[FiveM] Successfully initialized for build " << buildVersion << ":" << std::endl;
            std::cout << "  World: 0x" << std::hex << world << std::endl;
            std::cout << "  Viewport: 0x" << std::hex << viewport << std::endl;
            std::cout << "  Replay: 0x" << std::hex << replay << std::endl;
            std::cout << "  LocalPlayer: 0x" << std::hex << localplayer << std::dec << std::endl;
        }

        // The PED cache system is now thread-free and will be updated per frame
        // No initialization needed here - the cache manager will handle updates
        // when update() is called from the main loop
    }

    // Function to get current build version (useful for UI display)
    int GetCurrentBuildVersion() {
        return offset::buildVersion;
    }

    // Initialize PED cache (updated for new thread-free design)
    void InitializePedCache() {
        // Initialize the cache manager
        g_pedCacheManager.initialize();
        std::cout << "[FiveM] PED cache system initialized (thread-free design)" << std::endl;
    }
}