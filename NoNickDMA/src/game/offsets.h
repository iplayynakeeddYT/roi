#pragma once
#include <cstdint>
#include <string>

namespace FiveM {
    namespace offset {
        extern uintptr_t world, replay, viewport, camera, localplayer;
        extern uintptr_t boneList, boneMatrix;
        extern uintptr_t playerInfo, playerHealth, playerPosition;
        extern uintptr_t base;
        extern int buildVersion;

        // CPlayerInfo offsets
        const uintptr_t playerInfo_netId = 0x7C;  // Network ID offset within CPlayerInfo
    }

    // Structure to hold offset configurations for different builds
    struct BuildOffsets {
        int build;
        uintptr_t world_offset;
        uintptr_t replay_offset;
        uintptr_t viewport_offset;
        uintptr_t camera_offset;
        uintptr_t playerInfo_offset;
        uintptr_t boneList_offset;
        uintptr_t boneMatrix_offset;
        uintptr_t playerHealth_offset;
        uintptr_t playerPosition_offset;
    };

    // Get build version using date string detection
    int GetBuildVersion();

    // Check if offsets are supported for current build
    bool IsBuildSupported();

    // Get offset configuration for a specific build
    const BuildOffsets* GetOffsetsForBuild(int build);

    /*
    * OFFSET REFERENCE GUIDE:
    *
    * world_offset         - CWorld pointer (contains all game entities)
    * replay_offset        - CReplayInterface pointer (ped list at +0x18)
    * viewport_offset      - CViewport pointer (view matrix at +0x24C)
    * camera_offset        - CCamera pointer (camera data)
    * playerInfo_offset    - Offset from ped to CPlayerInfo
    * boneList_offset      - Offset from ped to bone list
    * boneMatrix_offset    - Offset from ped to bone matrix (usually 0x60)
    * playerHealth_offset  - Offset from ped to health value (usually 0x280)
    * playerPosition_offset- Offset from ped to position vector (usually 0x90)
    */
}