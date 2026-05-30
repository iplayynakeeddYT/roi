#include "offsets.h"
#include "../DMALibrary/Memory/Memory.h"
#include "../globals.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <vector>

namespace FiveM {
    namespace offset {
        uintptr_t world, replay, viewport, camera, localplayer;
        uintptr_t boneList, boneMatrix = 0x60;
        uintptr_t playerInfo, playerHealth = 0x280, playerPosition = 0x90;
        uintptr_t base;
        int buildVersion = 0;
    }

    // Comprehensive offset table for all known builds
    const BuildOffsets BUILD_OFFSETS[] = {
        // Build 372 (Jun  9 2015) - PLACEHOLDER
        {
            372,        // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 393 (Jun 30) - PLACEHOLDER
        {
            393,        // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 505 (Oct 13) - PLACEHOLDER
        {
            505,        // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1032 (Mar 31 2017) - PLACEHOLDER
        {
            1032,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1103 (Jun  9 2017) - PLACEHOLDER
        {
            1103,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1290 (Dec  6 2017) - PLACEHOLDER
        {
            1290,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1365 (Mar 14 2018) - PLACEHOLDER
        {
            1365,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1493 (Oct 14 2018) - PLACEHOLDER
        {
            1493,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1604 (Dec  5 2018) - PLACEHOLDER
        {
            1604,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1737 (Jul 23 2019) - PLACEHOLDER
        {
            1737,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 1868 (Dec 11 2019) - PLACEHOLDER
        {
            1868,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2060 (Aug  5 2020) - PLACEHOLDER
        {
            2060,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2189 (Dec 10 2020) - PLACEHOLDER
        {
            2189,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2372 (Jul 15 2021) - PLACEHOLDER
        {
            2372,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2545 (Dec 14 2021) - PLACEHOLDER
        {
            2545,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2612 (Apr 18 2022) - PLACEHOLDER
        {
            2612,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2699 (Jul 21 2022) - PLACEHOLDER
        {
            2699,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2802 (Dec  8 2022) - VERIFIED OFFSETS
        {
            2802,       // build
            0x1F5B820,  // world_offset     - CWorld*
            0x1F5B820,  // replay_offset    - CReplayInterface*
            0x1FBC100,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 2944 (Jun  8 2023) - VERIFIED OFFSETS
        {
            2944,       // build
            0x257BEA0,  // world_offset     - CWorld*
            0x1F42068,  // replay_offset    - CReplayInterface*
            0x1FEAAC0,  // viewport_offset  - CViewport*
            0x0,        // camera_offset    - CCamera* (not used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x0,        // boneList         - Ped->BoneList (not used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 3095 (Dec  8 2023) - VERIFIED OFFSETS
        {
            3095,       // build
            0x2593320,  // world_offset     - CWorld*
            0x1F58B58,  // replay_offset    - CReplayInterface*
            0x20019E0,  // viewport_offset  - CViewport*
            0x20025B8,  // camera_offset    - CCamera* (now used)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x48,       // boneList         - Ped->BoneList (now used)
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 3258 (Jun 20 2024) - VERIFIED OFFSETS
        {
            3258,       // build
            0x25B14B0,  // world_offset     - CWorld*
            0x1FBD4F0,  // replay_offset    - CReplayInterface*
            0x201DBA0,  // viewport_offset  - CViewport*
            0x201E7D0,  // camera_offset    - CCamera*
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x48,       // boneList         - Ped->BoneList
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 3323 (Sep 10 2024) - 
        {
            3323,       // build
            0x25C15B0,  // world_offset     - CWorld*
            0x1F85458,  // replay_offset    - CReplayInterface* 
            0x202DC50,  // viewport_offset  - CViewport* MAYBE????
            0x20025B8,  // camera_offset    - CCamera* (PLACEHOLDER - using 3095)
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x48,       // boneList         - Ped->BoneList
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },

        // Build 3407 (Dec  5 2024) - Updated
        {
            3407,       // build
            0x25D7108,  // world_offset     - CWorld* 
            0x1F9A9D8,  // replay_offset    - CReplayInterface* 
            0x20431C0,  // viewport_offset  - CViewport* 
            0x20440C8,  // camera_offset    - CCamera*
            0x10A8,     // playerInfo       - Ped->CPlayerInfo
            0x48,       // boneList         - Ped->BoneList
            0x60,       // boneMatrix       - Ped->BoneMatrix
            0x280,      // health           - Ped->Health
            0x90        // position         - Ped->Position
        },
    };

    // Function to get build version using date string detection
    int GetBuildVersion() {
        auto game_base = mem.GetBaseDaddy(g_validExecutable);

        std::cout << "[FiveM] Searching for build date pattern..." << std::endl;

        // Pattern to find the build date string reference
        const char* pattern = "\x48\x8D\x8E\x09\x02\x00\x00\x44\x8B\xC5\x33\xD2";
        const char* mask = "xxxxxxxxxxxx";

        // Search for the pattern
        uintptr_t pattern_location = 0;
        const size_t search_size = 0x5000000;
        const size_t chunk_size = 0x100000;

        for (size_t offset = 0; offset < search_size; offset += chunk_size) {
            std::vector<uint8_t> buffer(chunk_size);
            size_t bytes_to_read = (std::min)(chunk_size, search_size - offset);

            if (!mem.Read(game_base + offset, buffer.data(), bytes_to_read)) {
                continue;
            }

            // Search for pattern in this chunk
            for (size_t i = 0; i < bytes_to_read - 12; i++) {
                bool match = true;
                for (size_t j = 0; j < 12; j++) {
                    if (mask[j] == 'x' && buffer[i + j] != ((uint8_t*)pattern)[j]) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    pattern_location = game_base + offset + i;
                    std::cout << "[FiveM] Found pattern at: 0x" << std::hex << pattern_location << std::dec << std::endl;
                    break;
                }
            }

            if (pattern_location != 0) break;
        }

        if (pattern_location == 0) {
            std::cout << "[FiveM] Failed to find build date pattern" << std::endl;
            return 0;
        }

        // Get the location of the build string
        // Pattern offset + 20 contains a relative address
        uintptr_t location = pattern_location + 20;

        // Read the relative offset
        int32_t relative_offset;
        if (!mem.Read(location, &relative_offset, sizeof(int32_t))) {
            std::cout << "[FiveM] Failed to read relative offset" << std::endl;
            return 0;
        }

        // Calculate the actual build string address
        uintptr_t buildString_addr = location + relative_offset + 4;

        // Read the build date string
        char buildString[64];
        if (!mem.Read(buildString_addr, buildString, sizeof(buildString))) {
            std::cout << "[FiveM] Failed to read build date string" << std::endl;
            return 0;
        }

        buildString[sizeof(buildString) - 1] = '\0';
        std::cout << "[FiveM] Found build date: " << buildString << std::endl;

        // Map build date to version number using exact pattern matching
        int versionIdx = -1;

        // Note: Single-digit days have two spaces (e.g., "Dec  8 2023" not "Dec 8 2023")

        if (strncmp(buildString, "Dec  5 2024", 11) == 0)  // Two spaces before 5
        {
            versionIdx = 3407;
        }
        else if (strncmp(buildString, "Sep 10 2024", 11) == 0)
        {
            versionIdx = 3323;
        }
        else if (strncmp(buildString, "Jun 20 2024", 11) == 0)
        {
            versionIdx = 3258;
        }
        else if (strncmp(buildString, "Dec  8 2023", 11) == 0)  // Two spaces before 8
        {
            versionIdx = 3095;
        }
        else if (strncmp(buildString, "Jun  8 2023", 11) == 0)  // Two spaces before 8
        {
            versionIdx = 2944;
        }
        else if (strncmp(buildString, "Dec  8 2022", 11) == 0)  // Two spaces before 8
        {
            versionIdx = 2802;
        }
        else if (strncmp(buildString, "Jul 21 2022", 11) == 0)
        {
            versionIdx = 2699;
        }
        else if (strncmp(buildString, "Apr 18 2022", 11) == 0)
        {
            versionIdx = 2612;
        }
        else if (strncmp(buildString, "Dec 14 2021", 11) == 0)
        {
            versionIdx = 2545;
        }
        else if (strncmp(buildString, "Jul 15 2021", 11) == 0)
        {
            versionIdx = 2372;
        }
        else if (strncmp(buildString, "Dec 10 2020", 11) == 0)
        {
            versionIdx = 2189;
        }
        else if (strncmp(buildString, "Aug  5 2020", 11) == 0)  // Two spaces before 5
        {
            versionIdx = 2060;
        }
        else if (strncmp(buildString, "Dec 11 2019", 11) == 0)
        {
            versionIdx = 1868;
        }
        else if (strncmp(buildString, "Jul 23 2019", 11) == 0)
        {
            versionIdx = 1737;
        }
        else if (strncmp(buildString, "Dec  5 2018", 11) == 0)  // Two spaces before 5
        {
            versionIdx = 1604;
        }
        else if (strncmp(buildString, "Oct 14 2018", 11) == 0)
        {
            versionIdx = 1493; // .1
        }
        else if (strncmp(buildString, "Mar 14 2018", 11) == 0)
        {
            versionIdx = 1365;
        }
        else if (strncmp(buildString, "Dec  6 2017", 11) == 0)  // Two spaces before 6
        {
            versionIdx = 1290;
        }
        else if (strncmp(buildString, "Jun  9 2017", 11) == 0)  // Two spaces before 9
        {
            versionIdx = 1103;
        }
        else if (strncmp(buildString, "Mar 31 2017", 11) == 0)
        {
            versionIdx = 1032;
        }
        else if (strncmp(buildString, "Oct 13", 6) == 0)
        {
            versionIdx = 505;
        }
        else if (strncmp(buildString, "Jun 30", 6) == 0)
        {
            versionIdx = 393;
        }
        else if (strncmp(buildString, "Jun  9 2015", 11) == 0)  // Two spaces before 9
        {
            versionIdx = 372;
        }

        // early out if no version index matched
        if (versionIdx < 0)
        {
            std::cout << "[FiveM] No native mapping information found for game executable built on " << buildString << std::endl;
            return 0;
        }

        std::cout << "[FiveM] Detected build version: b" << versionIdx << std::endl;
        return versionIdx;
    }

    // Function to check if offsets are supported for current build
    bool IsBuildSupported() {
        // List of builds with verified offsets
        const int verified_builds[] = { 2802, 2944, 3095, 3258 };

        // Check if current build has verified offsets
        for (int verified : verified_builds) {
            if (offset::buildVersion == verified) {
                return true;
            }
        }

        // All other builds are using placeholder offsets
        return false;
    }

    // Get offset configuration for a specific build
    const BuildOffsets* GetOffsetsForBuild(int build) {
        for (const auto& build_offset : BUILD_OFFSETS) {
            if (build_offset.build == build) {
                return &build_offset;
            }
        }
        return nullptr;
    }
}