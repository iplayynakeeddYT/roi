#pragma once
#include <cstdint>
#include "../DMALibrary/Memory/Memory.h"
#include "offsets.h"

namespace FiveM {
    namespace Visibility {
        // Check if a ped is visible
        inline bool IsPedVisible(uintptr_t ped) {
            // If we don't have the visibility offset, assume visible
            if (offset::framecountlastvisible == 0) {
                return true;
            }

            // Read global frame count
            uint8_t global_frame_count;
            if (!mem.Read(offset::framecountlastvisible, &global_frame_count, sizeof(uint8_t))) {
                return true; // Assume visible on read failure
            }

            // Read ped's last visible frame count
            uint8_t ped_frame_count;
            if (!mem.Read(ped + offset::pedVisibilityOffset, &ped_frame_count, sizeof(uint8_t))) {
                return true; // Assume visible on read failure
            }

            // Ped is visible if frame counts match
            return global_frame_count == ped_frame_count;
        }

        // Batch visibility check for multiple peds
        inline void BatchCheckVisibility(const std::vector<uintptr_t>& peds, std::vector<bool>& visibility_results) {
            visibility_results.clear();
            visibility_results.resize(peds.size(), true); // Default to visible

            if (offset::framecountlastvisible == 0) {
                return; // All remain visible
            }

            // Read global frame count once
            uint8_t global_frame_count;
            if (!mem.Read(offset::framecountlastvisible, &global_frame_count, sizeof(uint8_t))) {
                return;
            }

            // Batch read all ped frame counts
            auto handle = mem.CreateScatterHandle();
            std::vector<uint8_t> ped_frame_counts(peds.size());

            for (size_t i = 0; i < peds.size(); ++i) {
                mem.AddScatterReadRequest(handle, peds[i] + offset::pedVisibilityOffset,
                    &ped_frame_counts[i], sizeof(uint8_t));
            }

            mem.ExecuteReadScatter(handle);
            mem.CloseScatterHandle(handle);

            // Compare frame counts
            for (size_t i = 0; i < peds.size(); ++i) {
                visibility_results[i] = (global_frame_count == ped_frame_counts[i]);
            }
        }
    }
}