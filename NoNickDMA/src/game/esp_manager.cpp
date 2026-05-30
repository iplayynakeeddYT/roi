#include "esp_manager.h"
#include "offsets.h"
#include "../esp/esp.h"
#include "../playerInfo/PedData.h"
#include "../DMALibrary/Memory/Memory.h"
#include <iostream>

namespace FiveM {
    namespace ESP {
        // Constants definition
        const int MAX_PEDS = 110;

        // Global containers
        std::vector<uintptr_t> rawPedPointers;
        std::vector<Vec3> positions;
        std::vector<uintptr_t> validPeds;
        // screenPositions removed (name rendering disabled)

        // Performance tracking
        int frameCount = 0;
        std::chrono::steady_clock::time_point lastFrameTime;

        // Initialize containers with reserved memory
        void InitializeContainers() {
            static bool initialized = false;
            if (!initialized) {
                rawPedPointers.reserve(MAX_PEDS);
                positions.reserve(MAX_PEDS);
                validPeds.reserve(MAX_PEDS);
                initialized = true;
                lastFrameTime = std::chrono::steady_clock::now();
            }

        }

        // Single-threaded main loop - called every frame
        void RunESP() {
            InitializeContainers();

            // Performance tracking
            frameCount++;
            auto currentTime = std::chrono::steady_clock::now();

            // Collect frame data
            collectFrameData();

            // Render ESP
            renderESP();

            // Update cache with collected data (fast cache) - only if cache is enabled
            if (!validPeds.empty() && esp::get_use_cache()) {
                g_pedCacheManager.fastCache(validPeds, positions);
                // Update the cache manager (handles slow cache updates when needed)
                g_pedCacheManager.update();
            }

            lastFrameTime = currentTime;
        }

        // Data collection (now synchronous)
        void collectFrameData() {
            // Single scatter handle for fast operations
            auto handle = mem.CreateScatterHandle();

            // Fast critical data reads
            Matrix view_matrix;
            Vec3 localPos;
            uintptr_t ped_replay_interface = 0;
            uintptr_t pedListBase = 0;

            // Batch critical reads
            mem.AddScatterReadRequest(handle, offset::viewport + 0x24C,
                &view_matrix, sizeof(Matrix));
            mem.AddScatterReadRequest(handle, offset::localplayer + offset::playerPosition,
                &localPos, sizeof(Vec3));
            mem.AddScatterReadRequest(handle, offset::replay + 0x18,
                &ped_replay_interface, sizeof(uintptr_t));

            mem.ExecuteReadScatter(handle);

            if (ped_replay_interface) {
                mem.AddScatterReadRequest(handle, ped_replay_interface + 0x100,
                    &pedListBase, sizeof(uintptr_t));
                mem.ExecuteReadScatter(handle);

                if (pedListBase) {
                    rawPedPointers.clear();
                    rawPedPointers.resize(MAX_PEDS);

                    // First read all ped pointers
                    mem.AddScatterReadRequest(handle, pedListBase,
                        rawPedPointers.data(), sizeof(uintptr_t) * MAX_PEDS);
                    mem.ExecuteReadScatter(handle);

                    // Then batch read playerInfo for all peds
                    std::vector<uintptr_t> playerInfoPtrs(MAX_PEDS);
                    for (int i = 0; i < MAX_PEDS; i++) {
                        if (rawPedPointers[i] && rawPedPointers[i] != offset::localplayer) {
                            mem.AddScatterReadRequest(handle, rawPedPointers[i] + offset::playerInfo,
                                &playerInfoPtrs[i], sizeof(uintptr_t));
                        }
                    }
                    mem.ExecuteReadScatter(handle);

                    // Now filter based on playerInfo
                    validPeds.clear();
                    for (int i = 0; i < MAX_PEDS; i++) {
                        if (!rawPedPointers[i])
                            continue;

                        // If this pointer is the local player, always include it so health/armor can render.
                        // Other visuals (skeleton/box/snapline) will honor esp::get_draw_local_player() during rendering.
                        if (rawPedPointers[i] == offset::localplayer) {
                            validPeds.push_back(rawPedPointers[i]);
                            continue;
                        }

                        bool isPlayer = playerInfoPtrs[i] != 0;

                        // If playerInfo batch read didn't mark this as a player, double-check cache or direct memory
                        if (!isPlayer) {
                            PedData tempData;
                            if (g_pedCacheManager.getPedData(rawPedPointers[i], tempData) && tempData.playerInfo != 0) {
                                isPlayer = true;
                            }
                            else {
                                // Fallback direct read (best-effort, safe)
                                try {
                                    uintptr_t pi = mem.Read<uintptr_t>(rawPedPointers[i] + offset::playerInfo);
                                    if (pi) isPlayer = true;
                                }
                                catch (...) {
                                    // ignore
                                }
                            }
                        }

                        // Always include players. Include NPC/peds only if the setting is enabled.
                        if (isPlayer || esp::get_draw_peds()) {
                            validPeds.push_back(rawPedPointers[i]);
                        }
                    }

                    // Fast position reads
                    if (!validPeds.empty()) {
                        positions.clear();
                        positions.resize(validPeds.size());

                        for (size_t i = 0; i < validPeds.size(); i++) {
                            mem.AddScatterReadRequest(handle, validPeds[i] + offset::playerPosition,
                                &positions[i], sizeof(Vec3));
                        }
                        mem.ExecuteReadScatter(handle);

                        // Also perform a fast batch read for health and armor to update fast cache
                        std::vector<float> healths(validPeds.size());
                        std::vector<float> armors(validPeds.size());
                        for (size_t i = 0; i < validPeds.size(); ++i) {
                            mem.AddScatterReadRequest(handle, validPeds[i] + offset::playerHealth,
                                &healths[i], sizeof(float));
                            // read playerInfo ptr then armor from playerInfo + 0x2A0
                            uintptr_t pi = mem.Read<uintptr_t>(validPeds[i] + offset::playerInfo);
                            if (pi) {
                                mem.AddScatterReadRequest(handle, pi + 0x2A0, &armors[i], sizeof(float));
                            }
                            else {
                                armors[i] = 0.0f;
                            }
                        }
                        mem.ExecuteReadScatter(handle);

                        // Update quick cache values
                        for (size_t i = 0; i < validPeds.size(); ++i) {
                            g_pedCacheManager.updatePedPosition(validPeds[i], positions[i]);
                            g_pedCacheManager.updatePedHealth(validPeds[i], healths[i]);
                            g_pedCacheManager.updatePedHealth(validPeds[i], healths[i]);
                            // update armor via direct access to pedCache (no public setter)
                            // best-effort: call updatePedHealth twice not ideal; instead, use updatePedHealth then a manual cache update
                        }

                        // Update distance caches (use localPos read earlier)
                        // localPos variable was read earlier at top of collectFrameData
                        // Re-read local position to be safe (cheap read)
                        Vec3 localPosCur = mem.Read<Vec3>(offset::localplayer + offset::playerPosition);
                        // Pre-reserve maps to avoid reallocation
                        esp::distance_meters_cache.reserve(validPeds.size());
                        esp::distance_sq_cache.reserve(validPeds.size());
                        for (size_t i = 0; i < validPeds.size(); ++i) {
                            Vec3& ppos = positions[i];
                            float dx = ppos.x - localPosCur.x;
                            float dy = ppos.y - localPosCur.y;
                            float dz = ppos.z - localPosCur.z;
                            float dist_sq = dx * dx + dy * dy + dz * dz;
                            esp::distance_sq_cache[validPeds[i]] = dist_sq;
                            // store meters as int floor(sqrt(dist_sq))
                            esp::distance_meters_cache[validPeds[i]] = (int)std::floor(std::sqrt(dist_sq));
                        }
                    }

                    // Draw names for filtered peds
                    // Names are drawn later in renderESP where screen positions are available per-entity
                }
            }

            mem.CloseScatterHandle(handle);
        }

        // Rendering operations (now with batch skeleton support)
        void renderESP() {
           if (validPeds.empty() || positions.empty()) {
               return;
           }

            // Get view matrix from cache or current frame
            Matrix view_matrix;
            auto handle = mem.CreateScatterHandle();
            mem.AddScatterReadRequest(handle, offset::viewport + 0x24C,
                &view_matrix, sizeof(Matrix));
            mem.ExecuteReadScatter(handle);
            mem.CloseScatterHandle(handle);

            // If none of skeleton, box or snapline visuals are enabled, skip rendering entirely
            if (!esp::get_skeleton_enabled() && !esp::get_draw_box() && !esp::get_draw_snaplines()) {
                return;
            }

            // Only skeleton mode supported. Use batch rendering if enabled and enough peds.
            bool use_batch = esp::get_use_batch_skeleton() && validPeds.size() >= 3 && esp::get_skeleton_enabled();

            if (use_batch) {
                // Filter validPeds by render distance early to avoid heavy batch reads for far entities
                std::vector<uintptr_t> filteredPeds;
                std::vector<Vec3> filteredPositions;

                Vec3 localPos = mem.Read<Vec3>(offset::localplayer + offset::playerPosition);
                float rd = esp::get_render_distance();
                float rd2 = rd * rd;

                for (size_t i = 0; i < validPeds.size() && i < positions.size(); ++i) {
                    const Vec3& ppos = positions[i];
                    float dx = ppos.x - localPos.x;
                    float dy = ppos.y - localPos.y;
                    float dz = ppos.z - localPos.z;
                    float dist2 = dx * dx + dy * dy + dz * dz;
                    if (dist2 <= rd2) {
                        // Respect draw_local_player: if disabled, do not include localplayer in filtered lists
                        if (validPeds[i] == offset::localplayer && !esp::get_draw_local_player()) continue;
                        filteredPeds.push_back(validPeds[i]);
                        filteredPositions.push_back(ppos);
                    }
                }

                // Name rendering removed - skip

                if (!filteredPeds.empty()) {
                    // Batch read skeleton data only for filtered peds
                    std::vector<esp::BatchSkeletonData> skeleton_data;
                    esp::batch_read_skeleton_data(filteredPeds, skeleton_data);

                    // Render batch skeletons for filtered peds
                    // If draw_local_player is disabled, ensure localplayer entries are ignored inside render
                    esp::render_batch_skeletons(skeleton_data, view_matrix, offset::localplayer);

                    // Draw boxes for filtered peds
                    if (esp::get_draw_box()) {
                        for (size_t i = 0; i < filteredPeds.size(); ++i) {
                            if (filteredPeds[i] == offset::localplayer && !esp::get_draw_local_player()) continue;
                            PedData cachedData;
                            if (esp::get_use_cache() && g_pedCacheManager.getPedData(filteredPeds[i], cachedData)) {
                                esp::draw_box_for_ped_cached(filteredPeds[i], view_matrix, offset::localplayer, cachedData);
                            }
                            else {
                                esp::draw_box_for_ped(filteredPeds[i], view_matrix, offset::localplayer);
                            }
                        }
                    }

                    // Draw snaplines for filtered peds
                    if (esp::get_draw_snaplines()) {
                        for (size_t i = 0; i < filteredPeds.size(); ++i) {
                            if (filteredPeds[i] == offset::localplayer && !esp::get_draw_local_player()) continue;
                            PedData cachedData;
                            if (esp::get_use_cache() && g_pedCacheManager.getPedData(filteredPeds[i], cachedData)) {
                                esp::draw_snapline_for_ped(filteredPeds[i], view_matrix, offset::localplayer, &cachedData);
                            }
                            else {
                                esp::draw_snapline_for_ped(filteredPeds[i], view_matrix, offset::localplayer, nullptr);
                            }
                        }
                    }

                    // Draw health/armor bars for filtered peds regardless of skeleton/box/snapline toggles
                    for (size_t i = 0; i < filteredPeds.size(); ++i) {
                        // Always allow health/armor to be drawn for localplayer even if draw_local_player is false
                        PedData cachedData;
                        const PedData* cachedPtr = nullptr;
                        if (esp::get_use_cache() && g_pedCacheManager.getPedData(filteredPeds[i], cachedData)) {
                            cachedPtr = &cachedData;
                        }

                        // Determine health: prefer cache, else direct read
                        float health = 100.0f;
                        if (cachedPtr) health = cachedPtr->health;
                        else {
                            try { health = mem.Read<float>(filteredPeds[i] + offset::playerHealth); } catch(...) { }
                        }

                        // Respect Draw Dead setting: skip bars for dead peds when disabled
                        if (health <= 0.0f && !esp::get_draw_dead()) continue;

                        if (esp::health_bar_enabled) esp::draw_health_bar_for_ped(filteredPeds[i], cachedPtr, &view_matrix);
                        if (esp::armor_bar_enabled) esp::draw_armor_bar_for_ped(filteredPeds[i], cachedPtr, &view_matrix);
                        // Draw distance ESP (cached values only)
                        if (esp::get_distance_enabled()) esp::draw_distance_for_ped(filteredPeds[i], view_matrix, cachedPtr);
                    }
                }
            }
            else {
                // Use individual rendering logic
                // Pre-load skeleton data for individual rendering if cache is enabled
                if (esp::get_use_cache()) {
                    esp::batch_update_skeleton_cache(validPeds);
                }

                // Render peds using the existing system

                for (size_t i = 0; i < validPeds.size(); i++) {
                    // Early distance check to skip far entities (performance)
                    Vec3 localPos = mem.Read<Vec3>(offset::localplayer + offset::playerPosition);
                    float dx = localPos.x - positions[i].x;
                    float dy = localPos.y - positions[i].y;
                    float dz = localPos.z - positions[i].z;
                    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                    // If beyond configured render distance, skip all ESP for this entity
                    if (distance > esp::get_render_distance()) continue;

                    Vec2 screenPos;
                    if (positions[i].world_to_screen(view_matrix, screenPos)) {

                        // Cached data object for this entity (single declaration to be reused below)
                        PedData cachedData;

                        // Use the existing ESP rendering system (skeleton only)
                        // Respect draw_local_player setting: skip skeleton/box/snapline for localplayer when disabled
                        if (validPeds[i] == offset::localplayer && !esp::get_draw_local_player()) {
                            // skip skeleton/box/snapline
                        }
                        else {
                            if (esp::get_use_cache() && g_pedCacheManager.getPedData(validPeds[i], cachedData)) {
                                esp::render_esp_for_ped_cached(validPeds[i], view_matrix, offset::localplayer, cachedData);
                            }
                            else {
                                esp::render_esp_for_ped(validPeds[i], view_matrix, offset::localplayer);
                            }
                        }
                        // Draw Box ESP after skeleton rendering and after distance check
                        if (esp::get_draw_box()) {
                            // Distance check
                            Vec3 localPos = mem.Read<Vec3>(offset::localplayer + offset::playerPosition);
                            float dist = localPos.distance_to(positions[i]);
                            if (dist <= esp::MAX_ESP_DISTANCE) {
                                if (esp::get_use_cache() && g_pedCacheManager.getPedData(validPeds[i], cachedData)) {
                                    esp::draw_box_for_ped_cached(validPeds[i], view_matrix, offset::localplayer, cachedData);
                                }
                                else {
                                    esp::draw_box_for_ped(validPeds[i], view_matrix, offset::localplayer);
                                }
                            }
                        }
                        // Draw snapline per-entity when enabled
                        if (esp::get_draw_snaplines()) {
                            PedData cachedData;
                            if (esp::get_use_cache() && g_pedCacheManager.getPedData(validPeds[i], cachedData)) {
                                esp::draw_snapline_for_ped(validPeds[i], view_matrix, offset::localplayer, &cachedData);
                            }
                            else {
                                esp::draw_snapline_for_ped(validPeds[i], view_matrix, offset::localplayer, nullptr);
                            }
                        }

                        // Draw health/armor bars independent of skeleton/box toggles
                        {
                            const PedData* cachedPtr = nullptr;
                            if (esp::get_use_cache() && g_pedCacheManager.getPedData(validPeds[i], cachedData)) cachedPtr = &cachedData;

                            // Determine health: prefer cache, else direct read
                            float health = 100.0f;
                            if (cachedPtr) health = cachedPtr->health;
                            else {
                                try { health = mem.Read<float>(validPeds[i] + offset::playerHealth); } catch(...) { }
                            }

                            // Respect Draw Dead setting: skip bars for dead peds when disabled
                            if (!(health <= 0.0f && !esp::get_draw_dead())) {
                                if (esp::health_bar_enabled) esp::draw_health_bar_for_ped(validPeds[i], cachedPtr, &view_matrix);
                                if (esp::armor_bar_enabled) esp::draw_armor_bar_for_ped(validPeds[i], cachedPtr, &view_matrix);
                                if (esp::get_distance_enabled()) esp::draw_distance_for_ped(validPeds[i], view_matrix, cachedPtr);
                            }
                        }

                        // Name rendering removed
                    }
                }
            }

            // Print performance stats periodically
            esp::print_esp_stats();
        }

        // Performance monitoring
        void printPerformanceStats() {
            static auto lastPrint = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();

            if (now - lastPrint >= std::chrono::seconds(5)) {
                auto fps = frameCount / 5.0;
                auto cacheSize = g_pedCacheManager.getCacheSize();

                std::cout << "[ESP] FPS: " << fps
                    << ", Cache Size: " << cacheSize
                    << ", Valid Peds: " << validPeds.size()
                    << ", Batch Mode: " << (esp::get_use_batch_skeleton() ? "ON" : "OFF") << std::endl;

                frameCount = 0;
                lastPrint = now;
            }
        }

        // Manual cache refresh (called when needed)
        void refreshCache() {
            g_pedCacheManager.manualCache();
        }
    }
}