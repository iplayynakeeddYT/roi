#pragma once

#include <unordered_map>
#include <chrono>
#include "../math/math.h"

// Define the structure for storing ped data
struct PedData {
    float health;
    float armor;
    Vec3 position_origin;
    uintptr_t playerInfo;
    std::chrono::steady_clock::time_point lastUpdate;
    bool isValid;

    PedData() : health(0.0f), position_origin{}, playerInfo(0),
        lastUpdate(std::chrono::steady_clock::now()), isValid(false), armor(0.0f) {
    }
};

// Single-threaded cache manager class
class PedCacheManager {
private:
    std::unordered_map<uintptr_t, PedData> pedCache;

    // Cache timing
    std::chrono::steady_clock::time_point lastSlowUpdate;
    static constexpr std::chrono::seconds SLOW_CACHE_INTERVAL{ 5 };

    // Internal cache methods
    void slowCache();

public:
    PedCacheManager();
    ~PedCacheManager();

    // Initialization
    void initialize();

    // Update method - call this every frame
    void update();

    // Cache access methods
    void fastCache(const std::vector<uintptr_t>& validPeds,
        const std::vector<Vec3>& positions);

    void manualCache(); // Full reinitialization

    // Reader methods
    bool getPedData(uintptr_t pedId, PedData& outData) const;
    std::vector<uintptr_t> getValidPedIds() const;
    size_t getCacheSize() const;

    // Writer methods
    void updatePedPosition(uintptr_t pedId, const Vec3& position);
    void updatePedHealth(uintptr_t pedId, float health);
    void removePed(uintptr_t pedId);
    void clearCache();

    // Cache management
    void cleanup(); // Remove invalid/old entries
};

// Global cache manager instance
extern PedCacheManager g_pedCacheManager;