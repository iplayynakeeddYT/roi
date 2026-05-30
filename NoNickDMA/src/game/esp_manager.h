#pragma once
#include <vector>
#include <chrono>
#include "../math/math.h"

namespace FiveM {
    namespace ESP {
        // Pre-allocated containers for performance
        extern std::vector<uintptr_t> rawPedPointers;
        extern std::vector<Vec3> positions;
        extern std::vector<uintptr_t> validPeds;
        // per-entity screen positions are no longer tracked for name rendering

        // Performance tracking
        extern int frameCount;
        extern std::chrono::steady_clock::time_point lastFrameTime;

        // Constants
        extern const int MAX_PEDS;

        // Core ESP functions
        void InitializeContainers();
        void RunESP(); // Main ESP loop (single-threaded)

        // Data collection and rendering functions
        void collectFrameData(); // Data collection
        void renderESP();        // Rendering

        // Cache management
        void refreshCache();     // Manual cache refresh

        // Performance monitoring
        void printPerformanceStats();
    }
}