// peddata.cpp
#include "peddata.h"
#include "../DMALibrary/Memory/Memory.h"
#include "../game/game.h"
#include "../globals.h"
#include "../esp/esp.h"
#include <iostream>

// Global cache manager instance
PedCacheManager g_pedCacheManager;

PedCacheManager::PedCacheManager()
    : lastSlowUpdate(std::chrono::steady_clock::now()) {
    pedCache.reserve(256); // Pre-allocate for better performance
}

PedCacheManager::~PedCacheManager() {
    // No thread to stop
}

void PedCacheManager::initialize() {
    // Just initialization, no thread starting
    lastSlowUpdate = std::chrono::steady_clock::now();
}

void PedCacheManager::update() {
    auto now = std::chrono::steady_clock::now();

    // Check if it's time for slow cache update
    if (now - lastSlowUpdate >= SLOW_CACHE_INTERVAL) {
        slowCache();
        lastSlowUpdate = now;
    }
}

void PedCacheManager::slowCache() {
    // Slow cache operations - player lists, health, static data, and bone data
    std::vector<float> healthValues;
    std::vector<uintptr_t> playerInfoValues;
    std::vector<uintptr_t> pedIds;

    // Collect valid ped IDs and prepare data containers
    for (auto& [pedId, pedData] : pedCache) {
        if (pedData.isValid) {
            pedIds.push_back(pedId);
        }
    }

    if (!pedIds.empty()) {
        healthValues.resize(pedIds.size());
        playerInfoValues.resize(pedIds.size());
            std::vector<float> armorValues;
            armorValues.resize(pedIds.size());

        auto handle = mem.CreateScatterHandle();

        // Batch read health and playerInfo
        for (size_t i = 0; i < pedIds.size(); ++i) {
            mem.AddScatterReadRequest(handle, pedIds[i] + FiveM::offset::playerHealth,
                &healthValues[i], sizeof(float));
                mem.AddScatterReadRequest(handle, pedIds[i] + FiveM::offset::playerInfo + 0x2A0,
                    &armorValues[i], sizeof(float));
            mem.AddScatterReadRequest(handle, pedIds[i] + FiveM::offset::playerInfo,
                &playerInfoValues[i], sizeof(uintptr_t));
        }

        mem.ExecuteReadScatter(handle);
        mem.CloseScatterHandle(handle);

        // Update cache with read values
        for (size_t i = 0; i < pedIds.size(); ++i) {
            auto it = pedCache.find(pedIds[i]);
            if (it != pedCache.end()) {
                it->second.health = healthValues[i];
                it->second.playerInfo = playerInfoValues[i];
                it->second.armor = armorValues[i];
            }
        }

        // Update bone cache for these peds as well
        esp::batch_update_bone_cache(pedIds);
    }

    // Clean up old entries
    cleanup();
}

void PedCacheManager::fastCache(const std::vector<uintptr_t>& validPeds,
    const std::vector<Vec3>& positions) {
    // Fast cache operations - positions and dynamic data
    auto now = std::chrono::steady_clock::now();

    // Update positions for current frame peds
    for (size_t i = 0; i < validPeds.size() && i < positions.size(); ++i) {
        uintptr_t pedId = validPeds[i];
        auto& pedData = pedCache[pedId];

        pedData.position_origin = positions[i];
        pedData.lastUpdate = now;
        pedData.isValid = true;
    }

    // Mark peds not in current frame as potentially invalid
    for (auto& [pedId, pedData] : pedCache) {
        bool foundInCurrentFrame = false;
        for (uintptr_t currentPed : validPeds) {
            if (currentPed == pedId) {
                foundInCurrentFrame = true;
                break;
            }
        }

        if (!foundInCurrentFrame) {
            // Don't immediately remove, but mark for cleanup
            auto timeSinceUpdate = now - pedData.lastUpdate;
            if (timeSinceUpdate > std::chrono::seconds(10)) {
                pedData.isValid = false;
            }
        }
    }
}

void PedCacheManager::manualCache() {
    // Full reinitialization - rare operation
    std::cout << "[PedCache] Manual cache reinitialization started\n";

    // Clear all cache data
    pedCache.clear();

    // Reset timing
    lastSlowUpdate = std::chrono::steady_clock::now();

    std::cout << "[PedCache] Manual cache reinitialization completed\n";
}

bool PedCacheManager::getPedData(uintptr_t pedId, PedData& outData) const {
    auto it = pedCache.find(pedId);
    if (it != pedCache.end() && it->second.isValid) {
        outData = it->second;
        return true;
    }
    return false;
}

std::vector<uintptr_t> PedCacheManager::getValidPedIds() const {
    std::vector<uintptr_t> validIds;
    validIds.reserve(pedCache.size());

    for (const auto& [pedId, pedData] : pedCache) {
        if (pedData.isValid) {
            validIds.push_back(pedId);
        }
    }

    return validIds;
}

size_t PedCacheManager::getCacheSize() const {
    return pedCache.size();
}

void PedCacheManager::updatePedPosition(uintptr_t pedId, const Vec3& position) {
    auto& pedData = pedCache[pedId];
    pedData.position_origin = position;
    pedData.lastUpdate = std::chrono::steady_clock::now();
    pedData.isValid = true;
}

void PedCacheManager::updatePedHealth(uintptr_t pedId, float health) {
    auto it = pedCache.find(pedId);
    if (it != pedCache.end()) {
        it->second.health = health;
        it->second.lastUpdate = std::chrono::steady_clock::now();
    }
}

void PedCacheManager::removePed(uintptr_t pedId) {
    pedCache.erase(pedId);
}

void PedCacheManager::clearCache() {
    pedCache.clear();
}

void PedCacheManager::cleanup() {
    // Remove invalid or old entries
    auto now = std::chrono::steady_clock::now();

    for (auto it = pedCache.begin(); it != pedCache.end(); ) {
        if (!it->second.isValid ||
            (now - it->second.lastUpdate) > std::chrono::seconds(15)) {
            it = pedCache.erase(it);
        }
        else {
            ++it;
        }
    }
}