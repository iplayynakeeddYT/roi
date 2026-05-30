#include "esp.h"
#include "../math/math.h"
#include "../game/game.h"
#include "../../ImGui/imgui.h"
#include <iostream>
#include "../playerInfo/PedData.h"
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <Memory/Memory.h>
#include <array>

// Initialize global instances
esp::BoneCache esp::bone_cache;
esp::ESPStats esp::esp_stats;

// ESP Configuration globals
esp::ESPMode esp::current_esp_mode = esp::ESPMode::SKELETON_BONES; // Default to skeleton
ImU32 esp::skeleton_color = IM_COL32(255, 0, 0, 255); // Red skeleton
float esp::line_thickness = 2.0f;
bool esp::use_cache = true;
bool esp::use_batch_skeleton = true;
bool esp::draw_peds = false;
bool esp::skeleton_enabled = false;
bool esp::health_bar_enabled = false;
ImU32 esp::health_bar_color = IM_COL32(0, 255, 0, 255);
bool esp::armor_bar_enabled = false;
ImU32 esp::armor_bar_color = IM_COL32(0, 160, 255, 255);
ImU32 esp::snapline_color = IM_COL32(255, 255, 255, 200);

esp::NamePosition esp::health_bar_position = esp::NamePosition::TOP;
esp::NamePosition esp::armor_bar_position = esp::NamePosition::TOP;

// Enhanced skeleton data structure for caching
struct CachedSkeletonData {
    std::vector<Vec3> bone_positions;  // Cache all bone positions
    std::chrono::steady_clock::time_point last_update;
    bool is_valid;

    CachedSkeletonData()
        : bone_positions(), last_update(std::chrono::steady_clock::now()), is_valid(false) {
        bone_positions.resize(9);
    }
};

// Compute screen-space AABB for ped using cached skeleton bones; returns false if cannot compute
static bool compute_screen_aabb(uintptr_t ped, const Matrix& view_matrix, ImVec2& out_min, ImVec2& out_max) {
    std::vector<Vec3> bones;
    if (!esp::get_skeleton_bones_for_ped(ped, bones, false) || bones.empty()) {
        return false;
    }

    Vec3 minv(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxv(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& b : bones) {
        if (b.x == 0 && b.y == 0 && b.z == 0) continue;
        if (b.x < minv.x) minv.x = b.x;
        if (b.y < minv.y) minv.y = b.y;
        if (b.z < minv.z) minv.z = b.z;
        if (b.x > maxv.x) maxv.x = b.x;
        if (b.y > maxv.y) maxv.y = b.y;
        if (b.z > maxv.z) maxv.z = b.z;
    }

    if (minv.x == FLT_MAX) return false;

    Vec3 corners[8] = {
        { minv.x, minv.y, minv.z }, { minv.x, minv.y, maxv.z }, { minv.x, maxv.y, minv.z }, { minv.x, maxv.y, maxv.z },
        { maxv.x, minv.y, minv.z }, { maxv.x, minv.y, maxv.z }, { maxv.x, maxv.y, minv.z }, { maxv.x, maxv.y, maxv.z }
    };

    bool any = false;
    float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
    for (int i = 0; i < 8; ++i) {
        Vec2 s;
        if (!corners[i].world_to_screen((Matrix&)view_matrix, s)) continue;
        any = true;
        if (s.x < left) left = s.x;
        if (s.y < top) top = s.y;
        if (s.x > right) right = s.x;
        if (s.y > bottom) bottom = s.y;
    }

    if (!any) return false;

    out_min = ImVec2(left, top);
    out_max = ImVec2(right, bottom);
    return true;
}

// Helper to compute bar rectangle based on position enum
static void compute_bar_rect(const Vec2& anchor, esp::NamePosition pos, float width, float height, ImVec2& out_min, ImVec2& out_max) {
    switch (pos) {
    case esp::NamePosition::TOP:
        out_min = ImVec2(anchor.x - width / 2.0f, anchor.y - 25.0f - height);
        out_max = ImVec2(anchor.x + width / 2.0f, anchor.y - 25.0f);
        break;
    case esp::NamePosition::BOTTOM:
        out_min = ImVec2(anchor.x - width / 2.0f, anchor.y + 5.0f);
        out_max = ImVec2(anchor.x + width / 2.0f, anchor.y + 5.0f + height);
        break;
    case esp::NamePosition::LEFT:
        out_min = ImVec2(anchor.x - 25.0f - width, anchor.y - height / 2.0f);
        out_max = ImVec2(anchor.x - 25.0f, anchor.y + height / 2.0f);
        break;
    case esp::NamePosition::RIGHT:
        out_min = ImVec2(anchor.x + 25.0f, anchor.y - height / 2.0f);
        out_max = ImVec2(anchor.x + 25.0f + width, anchor.y + height / 2.0f);
        break;
    default:
        out_min = ImVec2(anchor.x - width / 2.0f, anchor.y - 25.0f - height);
        out_max = ImVec2(anchor.x + width / 2.0f, anchor.y - 25.0f);
        break;
    }
}

// Draw health and armor helper functions (placed after compute_bar_rect to ensure visibility)
void esp::draw_health_bar_for_ped(uintptr_t ped, const PedData* cached, Matrix* viewport) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    // Determine anchor world position based on desired bar position
    Vec3 world_anchor;
    // Prefer cached bone positions when available
    std::vector<Vec3> bones;
    if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 0) {
        // head = bones[0]
        Vec3 head = bones[0];
        // approximate feet as head.y - 1.8m (player height) unless hip bone exists
        Vec3 feet = head;
        // If hip/spine bone available (7 or 6) use that to compute feet offset
        if (bones.size() > 6) {
            // use spine/hip to estimate body origin then subtract
            feet = bones[6];
        }
        // default anchor
        world_anchor = head;
        if (esp::health_bar_position == esp::NamePosition::BOTTOM) {
            // bottom under feet
            world_anchor = Vec3(feet.x, feet.y - 0.9f, feet.z);
        }
        else if (esp::health_bar_position == esp::NamePosition::LEFT) {
            // left beside left shoulder (bone 5 assumed left shoulder)
            if (bones.size() > 5) world_anchor = bones[5];
        }
        else if (esp::health_bar_position == esp::NamePosition::RIGHT) {
            // right beside right shoulder (bone 8 assumed right shoulder)
            if (bones.size() > 8) world_anchor = bones[8];
        }
        else {
            // TOP remains head
            world_anchor = head;
        }
    }
    else {
        // fallback to ped root position
        world_anchor = mem.Read<Vec3>(ped + FiveM::offset::playerPosition);
    }

    // Resolve view matrix
    Matrix view;
    if (viewport) view = *viewport;
    else view = mem.Read<Matrix>(FiveM::offset::viewport + 0x24C);

    // Try to compute screen-space AABB for the ped (used for syncing with box positions)
    ImVec2 box_min, box_max;
    bool have_box = compute_screen_aabb(ped, view, box_min, box_max);

    // Determine screen anchor
    Vec2 screen;
    if (esp::health_bar_position == esp::NamePosition::BOTTOM && have_box) {
        // place at center bottom of box
        screen.x = (box_min.x + box_max.x) * 0.5f;
        screen.y = box_max.y;
    }
    else if ((esp::health_bar_position == esp::NamePosition::LEFT || esp::health_bar_position == esp::NamePosition::RIGHT) && have_box) {
        // place at mid height on left/right side of box
        screen.x = (esp::health_bar_position == esp::NamePosition::LEFT) ? box_min.x : box_max.x;
        screen.y = (box_min.y + box_max.y) * 0.5f;
    }
    else {
        // fallback: project selected world_anchor
        if (!world_anchor.world_to_screen(view, screen)) return;
    }

    // Get health value
    float health = 0.0f;
    if (cached) health = cached->health;
    else {
        // Safe direct read
        try {
            health = mem.Read<float>(ped + FiveM::offset::playerHealth);
        }
        catch (...) { return; }
    }

    // Clamp health
    if (health <= 0.0f) return;
    float ratio = health / 100.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    // Compute rectangle (fixed size: width=34, height=4) - slightly larger for visibility
    ImVec2 min, max;
    Vec2 anchor(screen.x, screen.y);
    // Adjust anchor offsets for left/right to sit beside shoulders
    if (esp::health_bar_position == esp::NamePosition::LEFT) {
        anchor.x -= 10.0f; // shift left a bit
    }
    else if (esp::health_bar_position == esp::NamePosition::RIGHT) {
        anchor.x += 10.0f; // shift right a bit
    }
    // For left/right we draw a vertical bar synced with the box and with rounded caps
    if ((esp::health_bar_position == esp::NamePosition::LEFT || esp::health_bar_position == esp::NamePosition::RIGHT) && have_box) {
        // Vertical bar spanning from head (top) to feet (bottom) with small padding
        float bar_w = 5.0f;
        float padding = 2.0f;
        float top = box_min.y - padding;
        float bottom = box_max.y + padding;
        float base_offset = 12.0f;
        float cx = (esp::health_bar_position == esp::NamePosition::LEFT) ? (box_min.x - base_offset) : (box_max.x + base_offset);

        ImVec2 min_h(cx - bar_w / 2.0f, top);
        ImVec2 max_h(cx + bar_w / 2.0f, bottom);
        float rounding = bar_w * 0.5f;
        // Background
        draw_list->AddRectFilled(min_h, max_h, IM_COL32(0,0,0,200), rounding);
        // Foreground (fill from bottom up, scaled to the box height)
        float total_h = max_h.y - min_h.y;
        float filled_h = total_h * ratio;
        if (filled_h > 1.0f) {
            ImVec2 fg_min(min_h.x, max_h.y - filled_h);
            ImVec2 fg_max(max_h.x, max_h.y);
            draw_list->AddRectFilled(fg_min, fg_max, esp::health_bar_color, rounding);
        }
        draw_list->AddRect(min_h, max_h, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
    }
    else {
        // If requested position is left/right but we don't have a box, render a vertical side bar at the anchor
        if (esp::health_bar_position == esp::NamePosition::LEFT || esp::health_bar_position == esp::NamePosition::RIGHT) {
            float bar_w = 5.0f;
            float bar_h = 34.0f;
            float cx = anchor.x;
            float cy = anchor.y;

            ImVec2 bg_min = ImVec2(cx - bar_w/2.0f, cy - bar_h/2.0f);
            ImVec2 bg_max = ImVec2(cx + bar_w/2.0f, cy + bar_h/2.0f);
            float rounding = bar_w * 0.5f;
            draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(0,0,0,200), rounding);
            float filled_h = bar_h * ratio;
            if (filled_h > 1.0f) {
                ImVec2 fg_min(bg_min.x, bg_max.y - filled_h);
                ImVec2 fg_max(bg_max.x, bg_max.y);
                draw_list->AddRectFilled(fg_min, fg_max, esp::health_bar_color, rounding);
            }
            draw_list->AddRect(bg_min, bg_max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
        }
        else {
            compute_bar_rect(anchor, esp::health_bar_position, 34.0f, 4.0f, min, max);

            // Background with slight rounding
            float rounding = 2.5f;
            draw_list->AddRectFilled(min, max, IM_COL32(0,0,0,200), rounding);

            // Foreground
            ImVec2 fg_max = ImVec2(min.x + (max.x - min.x) * ratio, max.y);
            draw_list->AddRectFilled(min, fg_max, esp::health_bar_color, rounding);

            // Outline
            draw_list->AddRect(min, max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
        }
    }
}

void esp::draw_armor_bar_for_ped(uintptr_t ped, const PedData* cached, Matrix* viewport) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    // Determine anchor similarly to health bar but using armor bar position
    Vec3 world_anchor;
    std::vector<Vec3> bones;
    if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 0) {
        Vec3 head = bones[0];
        Vec3 feet = head;
        if (bones.size() > 6) feet = bones[6];
        world_anchor = head;
        if (esp::armor_bar_position == esp::NamePosition::BOTTOM) {
            world_anchor = Vec3(feet.x, feet.y - 0.9f, feet.z);
        }
        else if (esp::armor_bar_position == esp::NamePosition::LEFT) {
            if (bones.size() > 5) world_anchor = bones[5];
        }
        else if (esp::armor_bar_position == esp::NamePosition::RIGHT) {
            if (bones.size() > 8) world_anchor = bones[8];
        }
        else {
            world_anchor = head;
        }
    }
    else {
        world_anchor = mem.Read<Vec3>(ped + FiveM::offset::playerPosition);
    }

    Matrix view;
    if (viewport) view = *viewport;
    else view = mem.Read<Matrix>(FiveM::offset::viewport + 0x24C);

    ImVec2 box_min, box_max;
    bool have_box = compute_screen_aabb(ped, view, box_min, box_max);

    Vec2 screen;
    if (esp::armor_bar_position == esp::NamePosition::BOTTOM && have_box) {
        screen.x = (box_min.x + box_max.x) * 0.5f;
        screen.y = box_max.y;
    }
    else if ((esp::armor_bar_position == esp::NamePosition::LEFT || esp::armor_bar_position == esp::NamePosition::RIGHT) && have_box) {
        screen.x = (esp::armor_bar_position == esp::NamePosition::LEFT) ? box_min.x : box_max.x;
        screen.y = (box_min.y + box_max.y) * 0.5f;
    }
    else {
        if (!world_anchor.world_to_screen(view, screen)) return;
    }

    float armor = 0.0f;
    if (cached) armor = cached->armor;
    else {
        // playerInfo + 0x2A0
        try {
            uintptr_t playerInfo = mem.Read<uintptr_t>(ped + FiveM::offset::playerInfo);
            if (!playerInfo) return;
            armor = mem.Read<float>(playerInfo + 0x2A0);
        }
        catch (...) { return; }
    }

    if (armor <= 0.0f) return;
    float ratio = armor / 100.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    ImVec2 min, max;
    // Offset armor bar downward if health bar is also enabled (fixed height 3 now)
    float yoffset = esp::health_bar_enabled ? -(3.0f + 2.0f) : 0.0f;
    Vec2 anchor(screen.x, screen.y + yoffset);
    // Move armor further outward so it sits beside the health bar (side-by-side)
    if (esp::armor_bar_position == esp::NamePosition::LEFT) anchor.x -= 18.0f;
    if (esp::armor_bar_position == esp::NamePosition::RIGHT) anchor.x += 18.0f;

    // If we have a box, render a vertical bar on the side (armor offset further from box than health)
    if ((esp::armor_bar_position == esp::NamePosition::LEFT || esp::armor_bar_position == esp::NamePosition::RIGHT) && have_box) {
        float bar_w = 5.0f;
        float padding = 2.0f;
        float top = box_min.y - padding;
        float bottom = box_max.y + padding;
        float base_offset = 18.0f; // push armor slightly further out than health so it sits beside health
        float cx = (esp::armor_bar_position == esp::NamePosition::LEFT) ? (box_min.x - base_offset) : (box_max.x + base_offset);

        ImVec2 bg_min = ImVec2(cx - bar_w / 2.0f, top);
        ImVec2 bg_max = ImVec2(cx + bar_w / 2.0f, bottom);
        float rounding = bar_w * 0.5f;
        draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(0,0,0,200), rounding);

        float total_h = bg_max.y - bg_min.y;
        float filled_h = total_h * ratio;
        if (filled_h > 1.0f) {
            ImVec2 fg_min(bg_min.x, bg_max.y - filled_h);
            ImVec2 fg_max(bg_max.x, bg_max.y);
            draw_list->AddRectFilled(fg_min, fg_max, esp::armor_bar_color, rounding);
        }

        draw_list->AddRect(bg_min, bg_max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
    }
    else if (esp::armor_bar_position == esp::NamePosition::LEFT || esp::armor_bar_position == esp::NamePosition::RIGHT) {
        // Fallback: try to span from head to feet if bone positions available,
        // otherwise draw vertical side bar at anchor (armor further outward to avoid overlapping health)
        float bar_w = 5.0f;
        float padding = 2.0f;
        float cx = anchor.x;
        float top = 0.0f, bottom = 0.0f;
        bool span_computed = false;

        // Attempt to compute head/feet in screen-space via bones
        std::vector<Vec3> bones;
        if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 0) {
            Vec3 head = bones[0];
            Vec3 feet = head;
            if (bones.size() > 6) feet = bones[6];
            Vec2 head_s, feet_s;
            if (head.world_to_screen(view, head_s) && feet.world_to_screen(view, feet_s)) {
                top = head_s.y - padding;
                bottom = feet_s.y + padding;
                span_computed = true;
            }
        }

        // Fallback to fixed height if unable to compute span
        if (!span_computed) {
            float bar_h = 40.0f;
            top = anchor.y - bar_h / 2.0f;
            bottom = anchor.y + bar_h / 2.0f;
        }

        ImVec2 bg_min = ImVec2(cx - bar_w / 2.0f, top);
        ImVec2 bg_max = ImVec2(cx + bar_w / 2.0f, bottom);
        float rounding = bar_w * 0.5f;
        draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(0,0,0,200), rounding);

        float total_h = bg_max.y - bg_min.y;
        float filled_h = total_h * ratio;
        if (filled_h > 1.0f) {
            ImVec2 fg_min(bg_min.x, bg_max.y - filled_h);
            ImVec2 fg_max(bg_max.x, bg_max.y);
            draw_list->AddRectFilled(fg_min, fg_max, esp::armor_bar_color, rounding);
        }

        draw_list->AddRect(bg_min, bg_max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
    }
    else {
        // Fallback: if requested position is left/right but we don't have a box,
        // render a vertical bar at the anchor and push it outward if health is present.
        if (esp::armor_bar_position == esp::NamePosition::LEFT || esp::armor_bar_position == esp::NamePosition::RIGHT) {
            float bar_w = 5.0f;
            float bar_h = 40.0f;
            // If health also present, push armor slightly outward to avoid overlap
            float outward_offset = esp::health_bar_enabled ? 18.0f : 0.0f;
            float cx = anchor.x + ((esp::armor_bar_position == esp::NamePosition::LEFT) ? -outward_offset : outward_offset);
            float cy = anchor.y;

            ImVec2 bg_min = ImVec2(cx - bar_w / 2.0f, cy - bar_h / 2.0f);
            ImVec2 bg_max = ImVec2(cx + bar_w / 2.0f, cy + bar_h / 2.0f);
            float rounding = bar_w * 0.5f;
            draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(0,0,0,200), rounding);

            float filled_h = bar_h * ratio;
            if (filled_h > 1.0f) {
                ImVec2 fg_min(bg_min.x, bg_max.y - filled_h);
                ImVec2 fg_max(bg_max.x, bg_max.y);
                draw_list->AddRectFilled(fg_min, fg_max, esp::armor_bar_color, rounding);
            }

            draw_list->AddRect(bg_min, bg_max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
        }
        else {
            // Top/Bottom style remains a horizontal bar
            compute_bar_rect(anchor, esp::armor_bar_position, 34.0f, 4.0f, min, max);
            float rounding = 3.0f;
            draw_list->AddRectFilled(min, max, IM_COL32(0,0,0,200), rounding);
            ImVec2 fg_max = ImVec2(min.x + (max.x - min.x) * ratio, max.y);
            draw_list->AddRectFilled(min, fg_max, esp::armor_bar_color, rounding);
            draw_list->AddRect(min, max, IM_COL32(0,0,0,255), rounding, 0, 1.0f);
        }
    }

}

// Enhanced bone cache for skeleton
class EnhancedBoneCache {
public:  // Made public for batch updates
    std::unordered_map<uintptr_t, CachedSkeletonData> skeleton_cache; // Cache for skeleton data

private:
    static constexpr std::chrono::milliseconds SKELETON_CACHE_VALIDITY_MS{ 45 }; // Slightly longer for skeleton
    std::chrono::steady_clock::time_point last_cleanup;

public:
    EnhancedBoneCache() : last_cleanup(std::chrono::steady_clock::now()) {}

    // Get all bone positions for skeleton at once
    bool get_skeleton_bones(uintptr_t ped, std::vector<Vec3>& bone_positions, bool force_refresh = false) {
        auto now = std::chrono::steady_clock::now();

        auto it = skeleton_cache.find(ped);
        if (!force_refresh && it != skeleton_cache.end() && it->second.is_valid) {
            auto age = now - it->second.last_update;
            if (age < SKELETON_CACHE_VALIDITY_MS) {
                bone_positions = it->second.bone_positions;
                esp::esp_stats.cache_hits++;
                return true;
            }
        }

        esp::esp_stats.cache_misses++;

        // Read all skeleton bones in one batch operation
        Matrix bone_matrix = esp::bone_cache.get_bone_matrix(ped, force_refresh);

        // Read all bone offsets we need for skeleton
        std::vector<int> bone_indices = { 0, 3, 4, 5, 6, 7, 8 }; // All bones needed for skeleton
        std::vector<Vector3> bone_offsets(bone_indices.size());

        auto handle = mem.CreateScatterHandle();
        for (size_t i = 0; i < bone_indices.size(); ++i) {
            mem.AddScatterReadRequest(handle, ped + (esp::BONE_ARRAY_BASE + esp::BONE_SIZE * bone_indices[i]),
                &bone_offsets[i], sizeof(Vector3));
        }
        mem.ExecuteReadScatter(handle);
        mem.CloseScatterHandle(handle);

        // Transform all bones at once
        bone_positions.resize(9); // Ensure proper size
        for (size_t i = 0; i < bone_indices.size(); ++i) {
            DirectX::SimpleMath::Vector3 boneVec(bone_offsets[i].x, bone_offsets[i].y, bone_offsets[i].z);
            DirectX::SimpleMath::Vector3 transformedBoneVec = DirectX::XMVector3Transform(boneVec, bone_matrix);
            bone_positions[bone_indices[i]] = Vec3(transformedBoneVec.x, transformedBoneVec.y, transformedBoneVec.z);
        }

        // Cache the results
        auto& cached_data = skeleton_cache[ped];
        cached_data.bone_positions = bone_positions; // Store bone positions in cache
        cached_data.last_update = now;
        cached_data.is_valid = true;

        esp::esp_stats.memory_reads += bone_indices.size();
        return true; // Successfully retrieved bone positions
    }

    void cleanup_skeleton_cache() {
        auto now = std::chrono::steady_clock::now();

        if (now - last_cleanup < std::chrono::seconds(3)) {
            return;
        }

        for (auto it = skeleton_cache.begin(); it != skeleton_cache.end(); ) {
            auto age = now - it->second.last_update;
            if (age > std::chrono::seconds(15)) {
                it = skeleton_cache.erase(it);
            }
            else {
                ++it;
            }
        }

        last_cleanup = now;
    }

    size_t get_skeleton_cache_size() const { return skeleton_cache.size(); }
    void clear_skeleton_cache() { skeleton_cache.clear(); }
};

// Global enhanced bone cache instance
static EnhancedBoneCache enhanced_bone_cache;

// Bone Cache Implementation (keeping your existing logic)
Matrix esp::BoneCache::get_bone_matrix(uintptr_t ped, bool force_refresh) {
    auto now = std::chrono::steady_clock::now();

    auto it = cached_bone_data.find(ped);
    if (!force_refresh && it != cached_bone_data.end() && it->second.is_valid) {
        auto age = now - it->second.last_update;
        if (age < CACHE_VALIDITY_MS) {
            esp_stats.cache_hits++;
            return it->second.bone_matrix;
        }
    }

    esp_stats.cache_misses++;
    esp_stats.memory_reads++;

    Matrix bone_matrix;
    auto handle = mem.CreateScatterHandle();
    mem.AddScatterReadRequest(handle, ped + BONE_MATRIX_OFFSET, &bone_matrix, sizeof(Matrix));
    mem.ExecuteReadScatter(handle);
    mem.CloseScatterHandle(handle);

    auto& cached_data = cached_bone_data[ped];
    cached_data.bone_matrix = bone_matrix;
    cached_data.last_update = now;
    cached_data.is_valid = true;

    return bone_matrix;
}

Vec3 esp::BoneCache::get_bone_position(uintptr_t ped, int bone_position, bool force_refresh) {
    auto now = std::chrono::steady_clock::now();

    if (bone_position == 0) {
        auto it = cached_bone_data.find(ped);
        if (!force_refresh && it != cached_bone_data.end() && it->second.is_valid) {
            auto age = now - it->second.last_update;
            if (age < CACHE_VALIDITY_MS) {
                esp_stats.cache_hits++;
                return it->second.head_position;
            }
        }
    }

    esp_stats.cache_misses++;
    esp_stats.memory_reads++;

    Matrix bone_matrix = get_bone_matrix(ped, force_refresh);

    Vector3 bone_offset;
    auto handle = mem.CreateScatterHandle();
    mem.AddScatterReadRequest(handle, ped + (BONE_ARRAY_BASE + BONE_SIZE * bone_position),
        reinterpret_cast<void*>(&bone_offset), sizeof(Vector3));
    mem.ExecuteReadScatter(handle);
    mem.CloseScatterHandle(handle);

    DirectX::SimpleMath::Vector3 boneVec(bone_offset.x, bone_offset.y, bone_offset.z);
    DirectX::SimpleMath::Vector3 transformedBoneVec = DirectX::XMVector3Transform(boneVec, bone_matrix);
    Vec3 result(transformedBoneVec.x, transformedBoneVec.y, transformedBoneVec.z);

    if (bone_position == 0) {
        auto& cached_data = cached_bone_data[ped];
        cached_data.head_position = result;
        cached_data.last_update = now;
        cached_data.is_valid = true;
    }

    return result;
}

void esp::BoneCache::update_bone_data(uintptr_t ped, const Matrix& bone_matrix, const Vec3& head_pos) {
    auto now = std::chrono::steady_clock::now();
    auto& cached_data = cached_bone_data[ped];

    cached_data.bone_matrix = bone_matrix;
    cached_data.head_position = head_pos;
    cached_data.last_update = now;
    cached_data.is_valid = true;
}

void esp::BoneCache::cleanup_old_entries() {
    auto now = std::chrono::steady_clock::now();

    if (now - last_cleanup < CLEANUP_INTERVAL) {
        return;
    }

    for (auto it = cached_bone_data.begin(); it != cached_bone_data.end(); ) {
        auto age = now - it->second.last_update;
        if (age > std::chrono::seconds(10)) {
            it = cached_bone_data.erase(it);
        }
        else {
            ++it;
        }
    }

    last_cleanup = now;
}

bool esp::BoneCache::is_data_valid(uintptr_t ped) const {
    auto it = cached_bone_data.find(ped);
    if (it == cached_bone_data.end()) return false;

    auto now = std::chrono::steady_clock::now();
    auto age = now - it->second.last_update;
    return it->second.is_valid && age < CACHE_VALIDITY_MS;
}


// ESP Mode Management
void esp::set_esp_mode(ESPMode mode) {
    current_esp_mode = mode;
}

esp::ESPMode esp::get_esp_mode() {
    return current_esp_mode;
}

const char* esp::get_esp_mode_name(ESPMode mode) {
    // Only skeleton mode is supported now
    switch (mode) {
    case ESPMode::SKELETON_BONES: return "Skeleton Bones";
    default: return "Skeleton Bones";
    }
}

std::vector<const char*> esp::get_esp_mode_names() {
    return { "Skeleton Bones" };
}

// NEW: Batch skeleton reading implementation
void esp::batch_read_skeleton_data(const std::vector<uintptr_t>& peds, std::vector<BatchSkeletonData>& out_data) {
    if (peds.empty()) return;

    // Pre-allocate output data
    out_data.clear();
    out_data.resize(peds.size());

    // Define which bones we need for skeleton
    const std::vector<int> skeleton_bones = { 0, 3, 4, 5, 6, 7, 8 };
    const size_t num_bones = skeleton_bones.size();

    // Create single scatter handle for ALL reads
    auto handle = mem.CreateScatterHandle();

    // First pass: Add all bone matrix read requests
    for (size_t i = 0; i < peds.size(); ++i) {
        out_data[i].ped = peds[i];
        mem.AddScatterReadRequest(handle, peds[i] + BONE_MATRIX_OFFSET,
            &out_data[i].bone_matrix, sizeof(Matrix));
    }

    // Second pass: Add all bone offset read requests
    for (size_t ped_idx = 0; ped_idx < peds.size(); ++ped_idx) {
        for (size_t bone_idx = 0; bone_idx < skeleton_bones.size(); ++bone_idx) {
            int bone_id = skeleton_bones[bone_idx];
            mem.AddScatterReadRequest(handle,
                peds[ped_idx] + (BONE_ARRAY_BASE + BONE_SIZE * bone_id),
                &out_data[ped_idx].bone_offsets[bone_id],
                sizeof(Vector3));
        }
    }

    // Execute ALL reads in one operation
    mem.ExecuteReadScatter(handle);
    mem.CloseScatterHandle(handle);

    // Mark valid entries
    for (auto& data : out_data) {
        data.valid = true;
    }

    // Update stats
    esp_stats.memory_reads += peds.size() * (1 + skeleton_bones.size());
    esp_stats.batch_reads++;
}

// NEW: Batch skeleton rendering
void esp::render_batch_skeletons(const std::vector<BatchSkeletonData>& skeleton_data,
    Matrix viewport, uintptr_t localplayer) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Bone connections for skeleton
    static const int bone_connections[][2] = {
        { 0, 7 }, { 7, 6 }, { 7, 5 }, { 7, 8 }, { 8, 3 }, { 8, 4 }
    };

    // Pre-calculate local player position for distance calculations
    Vec3 local_pos = mem.Read<Vec3>(localplayer + 0x90);

    for (const auto& data : skeleton_data) {
        if (!data.valid) continue;

        // Transform all bones for this ped
        std::vector<Vec3> world_positions(9);
        std::vector<Vec2> screen_positions(9);
        std::vector<bool> on_screen(9, false);

        // Batch transform bones to world space
        for (int bone_id : {0, 3, 4, 5, 6, 7, 8}) {
            DirectX::SimpleMath::Vector3 boneVec(
                data.bone_offsets[bone_id].x,
                data.bone_offsets[bone_id].y,
                data.bone_offsets[bone_id].z
            );
            DirectX::SimpleMath::Vector3 transformedBoneVec =
                DirectX::XMVector3Transform(boneVec, data.bone_matrix);

            world_positions[bone_id] = Vec3(
                transformedBoneVec.x,
                transformedBoneVec.y,
                transformedBoneVec.z
            );

            // Convert to screen space
            on_screen[bone_id] = world_positions[bone_id].world_to_screen(
                viewport, screen_positions[bone_id]
            );
        }

        // Get health for coloring (from cache if available). Fallback to direct read.
        float health = 100.0f;
        PedData cached_ped_data;
        if (g_pedCacheManager.getPedData(data.ped, cached_ped_data)) {
            health = cached_ped_data.health;
        }
        else {
            // Best-effort direct memory read
            try { health = mem.Read<float>(data.ped + FiveM::offset::playerHealth); } catch(...) { }
        }

        // Respect Draw Dead setting: if dead and draw_dead is disabled, skip this entity
        if (health <= 0.0f && !esp::get_draw_dead()) continue;

        // Determine skeleton color based on health
        ImU32 current_skeleton_color = skeleton_color;
        if (health < 50.0f) current_skeleton_color = IM_COL32(255, 255, 0, 255);
        if (health < 25.0f) current_skeleton_color = IM_COL32(255, 100, 0, 255);
        if (health <= 0.0f) current_skeleton_color = esp::dead_skeleton_color;

        // Draw skeleton connections
        for (const auto& connection : bone_connections) {
            int bone1 = connection[0];
            int bone2 = connection[1];

            if (on_screen[bone1] && on_screen[bone2]) {
                draw_list->AddLine(
                    ImVec2(screen_positions[bone1].x, screen_positions[bone1].y),
                    ImVec2(screen_positions[bone2].x, screen_positions[bone2].y),
                    current_skeleton_color,
                    line_thickness
                );

                // Draw joint circles
                if (health > 0.0f) {
                    float joint_radius = line_thickness * 0.75f;
                    draw_list->AddCircleFilled(
                        ImVec2(screen_positions[bone1].x, screen_positions[bone1].y),
                        joint_radius,
                        current_skeleton_color
                    );
                    draw_list->AddCircleFilled(
                        ImVec2(screen_positions[bone2].x, screen_positions[bone2].y),
                        joint_radius,
                        current_skeleton_color
                    );
                }
            }
        }

        // Draw head indicator
        if (on_screen[0]) {
            float distance = local_pos.distance_to(world_positions[0]);
            float distance_factor = 50.0f / distance;
            if (distance_factor < 0.2f) distance_factor = 0.2f;
            if (distance_factor > 2.0f) distance_factor = 2.0f;

            float head_radius = 4.0f * distance_factor;
            if (head_radius < 1.0f) head_radius = 1.0f;
            if (head_radius > 8.0f) head_radius = 8.0f;

            draw_list->AddCircle(
                ImVec2(screen_positions[0].x, screen_positions[0].y),
                head_radius,
                current_skeleton_color,
                12,
                head_radius * 0.25f
            );
        }
    }

    // Update cache with transformed data
    for (const auto& data : skeleton_data) {
        if (data.valid) {
            std::vector<Vec3> bone_positions(9);
            for (int bone_id : {0, 3, 4, 5, 6, 7, 8}) {
                DirectX::SimpleMath::Vector3 boneVec(
                    data.bone_offsets[bone_id].x,
                    data.bone_offsets[bone_id].y,
                    data.bone_offsets[bone_id].z
                );
                DirectX::SimpleMath::Vector3 transformedBoneVec =
                    DirectX::XMVector3Transform(boneVec, data.bone_matrix);
                bone_positions[bone_id] = Vec3(
                    transformedBoneVec.x,
                    transformedBoneVec.y,
                    transformedBoneVec.z
                );
            }

            // Update enhanced bone cache
            auto& cached_data = enhanced_bone_cache.skeleton_cache[data.ped];
            cached_data.bone_positions = bone_positions;
            cached_data.last_update = std::chrono::steady_clock::now();
            cached_data.is_valid = true;
        }
    }
}

// NEW: Batch skeleton ESP rendering function
void esp::render_skeleton_esp_batch() {
    // Get all valid peds
    std::vector<uintptr_t> valid_peds = g_pedCacheManager.getValidPedIds();
    if (valid_peds.empty()) return;

    // Get viewport matrix once
    Matrix view_matrix;
    auto handle = mem.CreateScatterHandle();
    mem.AddScatterReadRequest(handle, FiveM::offset::viewport + 0x24C,
        &view_matrix, sizeof(Matrix));
    mem.ExecuteReadScatter(handle);
    mem.CloseScatterHandle(handle);

    // Batch read all skeleton data
    std::vector<BatchSkeletonData> skeleton_data;
    batch_read_skeleton_data(valid_peds, skeleton_data);

    // Render all skeletons in one pass
    render_batch_skeletons(skeleton_data, view_matrix, FiveM::offset::localplayer);

    // Clean up old cache entries
    enhanced_bone_cache.cleanup_skeleton_cache();
}

// head-circle removed

// Main rendering dispatcher - skeleton only
void esp::render_esp_for_ped(uintptr_t ped, Matrix viewport, uintptr_t localplayer) {
    draw_skeleton(ped, viewport, localplayer);
}

void esp::render_esp_for_ped_cached(uintptr_t ped, Matrix viewport, uintptr_t localplayer, const PedData& cached_ped_data) {
    draw_skeleton_cached(ped, viewport, localplayer, cached_ped_data);
}

// SKELETON ESP (modified to use new health bar)
void esp::draw_skeleton_cached(uintptr_t ped, Matrix viewport, uintptr_t localplayer, const PedData& cached_ped_data) {
    if (!get_skeleton_enabled()) return;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Bone connections for skeleton (correct layout)
    static const int bone_connections[][2] = {
        { 0, 7 },  // Head to neck
        { 7, 6 },  // Neck to spine
        { 7, 5 },  // Neck to left shoulder  
        { 7, 8 },  // Neck to right shoulder
        { 8, 3 },  // Right shoulder to right hand
        { 8, 4 }   // Right shoulder to left hand
    };

    // Get all bone positions from cache
    std::vector<Vec3> bone_positions;
    if (!enhanced_bone_cache.get_skeleton_bones(ped, bone_positions)) {
        return; // Failed to get bone data
    }

        // Use configured skeleton color but allow dead color override if dead
        float health = cached_ped_data.health;
        if (health <= 0.0f && !esp::get_draw_dead()) return;
        ImU32 current_skeleton_color = esp::skeleton_color;
        if (health <= 0.0f) current_skeleton_color = esp::dead_skeleton_color;

    // Pre-calculate all screen positions to avoid redundant world_to_screen calls
    std::vector<Vec2> screen_positions(bone_positions.size());
    std::vector<bool> on_screen(bone_positions.size(), false);

    for (size_t i = 0; i < bone_positions.size(); ++i) {
        on_screen[i] = bone_positions[i].world_to_screen(viewport, screen_positions[i]);
    }

    // Draw skeleton connections with enhanced visuals
    bool drew_head_info = false;
    Vec2 head_screen_pos;

    for (int i = 0; i < 6; ++i) {
        int bone1_idx = bone_connections[i][0];
        int bone2_idx = bone_connections[i][1];

        if (bone1_idx < bone_positions.size() && bone2_idx < bone_positions.size() &&
            on_screen[bone1_idx] && on_screen[bone2_idx]) {

            Vec2 bone1_screen = screen_positions[bone1_idx];
            Vec2 bone2_screen = screen_positions[bone2_idx];

            // Enhanced line drawing with health-based thickness
            float current_thickness = line_thickness;
            if (cached_ped_data.health > 75.0f) {
                current_thickness *= 1.2f; // Thicker lines for healthy targets
            }
            else if (cached_ped_data.health < 25.0f) {
                current_thickness *= 0.8f; // Thinner lines for weak targets
            }

            draw_list->AddLine(
                ImVec2(bone1_screen.x, bone1_screen.y),
                ImVec2(bone2_screen.x, bone2_screen.y),
                current_skeleton_color,
                current_thickness
            );

            // Draw joint circles for better visibility
            if (cached_ped_data.health > 0.0f) {
                float joint_radius = current_thickness * 0.75f;
                draw_list->AddCircleFilled(
                    ImVec2(bone1_screen.x, bone1_screen.y),
                    joint_radius,
                    current_skeleton_color
                );
                draw_list->AddCircleFilled(
                    ImVec2(bone2_screen.x, bone2_screen.y),
                    joint_radius,
                    current_skeleton_color
                );
            }

            // Enhanced head info drawing (only once)
            if (i == 0 && bone1_idx == 0 && !drew_head_info) { // Head bone
                head_screen_pos = bone1_screen;
                drew_head_info = true;


                // Enhanced head marker for skeleton mode - distance-based size
                Vec3 local_pos = mem.Read<Vec3>(localplayer + 0x90);
                float distance = local_pos.distance_to(bone_positions[0]);

                // Calculate head circle size based on distance (closer = larger, further = smaller)
                float base_radius = 4.0f;
                float min_radius = 1.0f;
                float max_radius = 8.0f;

                // Manual clamp implementation to avoid std::clamp issues
                float distance_factor = 50.0f / distance;
                if (distance_factor < 0.2f) distance_factor = 0.2f;
                if (distance_factor > 2.0f) distance_factor = 2.0f;

                float head_radius = base_radius * distance_factor;
                if (head_radius < min_radius) head_radius = min_radius;
                if (head_radius > max_radius) head_radius = max_radius;

                float line_thickness_head = head_radius * 0.25f;
                if (line_thickness_head < 1.0f) line_thickness_head = 1.0f;

                draw_list->AddCircle(
                    ImVec2(bone1_screen.x, bone1_screen.y),
                    head_radius,
                    current_skeleton_color,
                    12,
                    line_thickness_head
                );
            }
        }
    }

    // Cleanup old cache entries periodically
    enhanced_bone_cache.cleanup_skeleton_cache();

    // health/armor bars are drawn centrally in esp_manager to be independent of skeleton state
}

bool esp::get_skeleton_bones_for_ped(uintptr_t ped, std::vector<Vec3>& bone_positions, bool force_refresh)
{
    return enhanced_bone_cache.get_skeleton_bones(ped, bone_positions, force_refresh);
}

void esp::draw_skeleton(uintptr_t ped, Matrix viewport, uintptr_t localplayer) {
    if (!get_skeleton_enabled()) return;
    // Try to get cached data first
    PedData cached_ped_data;
    if (g_pedCacheManager.getPedData(ped, cached_ped_data)) {
        draw_skeleton_cached(ped, viewport, localplayer, cached_ped_data);
        return;
    }

    // Fallback to direct memory access with optimizations
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    static const int bone_connections[][2] = {
        { 0, 7 }, { 7, 6 }, { 7, 5 }, { 7, 8 }, { 8, 3 }, { 8, 4 }
    };

    // Get all bone positions in one batch
    std::vector<Vec3> bone_positions;
    if (!enhanced_bone_cache.get_skeleton_bones(ped, bone_positions, true)) {
        return;
    }

    // Get health directly (best-effort)
    float health = 100.0f;
    try { health = mem.Read<float>(ped + 0x280); } catch(...) { }

    if (health <= 0.0f && !esp::get_draw_dead()) return;

    // Pre-calculate screen positions
    std::vector<Vec2> screen_positions(bone_positions.size());
    std::vector<bool> on_screen(bone_positions.size(), false);

    for (size_t i = 0; i < bone_positions.size(); ++i) {
        on_screen[i] = bone_positions[i].world_to_screen(viewport, screen_positions[i]);
    }

    // Draw skeleton with health-based coloring
    ImU32 current_color = skeleton_color;
    if (health < 50.0f) current_color = IM_COL32(255, 255, 0, 255);
    if (health < 25.0f) current_color = IM_COL32(255, 100, 0, 255);
    if (health <= 0.0f) current_color = esp::dead_skeleton_color;

    for (int i = 0; i < 6; ++i) {
        int bone1_idx = bone_connections[i][0];
        int bone2_idx = bone_connections[i][1];

        if (bone1_idx < bone_positions.size() && bone2_idx < bone_positions.size() &&
            on_screen[bone1_idx] && on_screen[bone2_idx]) {

            draw_list->AddLine(
                ImVec2(screen_positions[bone1_idx].x, screen_positions[bone1_idx].y),
                ImVec2(screen_positions[bone2_idx].x, screen_positions[bone2_idx].y),
                current_color, line_thickness
            );
        }
    }
}

// Keep the old enhanced health info function for backward compatibility
void esp::draw_enhanced_health_info(const Vec2& position, float health) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Health bar
    float health_ratio = health / 100.0f;
    if (health_ratio < 0.0f) health_ratio = 0.0f;
    if (health_ratio > 1.0f) health_ratio = 1.0f;

    float bar_width = 40.0f;
    float bar_height = 5.0f;

    // Background bar
    draw_list->AddRectFilled(
        ImVec2(position.x - bar_width / 2, position.y - 25),
        ImVec2(position.x + bar_width / 2, position.y - 25 + bar_height),
        IM_COL32(0, 0, 0, 180)
    );

    // Health bar with gradient coloring
    ImU32 health_color;
    if (health > 75.0f) {
        health_color = IM_COL32(0, 255, 0, 255);     // Green
    }
    else if (health > 50.0f) {
        health_color = IM_COL32(255, 255, 0, 255);   // Yellow
    }
    else if (health > 25.0f) {
        health_color = IM_COL32(255, 165, 0, 255);   // Orange
    }
    else {
        health_color = IM_COL32(255, 0, 0, 255);     // Red
    }

    draw_list->AddRectFilled(
        ImVec2(position.x - bar_width / 2, position.y - 25),
        ImVec2(position.x - bar_width / 2 + bar_width * health_ratio, position.y - 25 + bar_height),
        health_color
    );

    // Health text
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.0f", health);
    draw_list->AddText(
        ImVec2(position.x - 10, position.y - 18),
        IM_COL32(255, 255, 255, 255),
        buffer
    );
}

// Utility functions
void esp::draw_health_info(const Vec2& position, float health) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1f", health);

    // Color based on health
    ImU32 text_color = IM_COL32(255, 255, 255, 255); // White
    if (health < 50.0f) text_color = IM_COL32(255, 255, 0, 255); // Yellow
    if (health < 25.0f) text_color = IM_COL32(255, 0, 0, 255); // Red

    draw_list->AddText(ImVec2(position.x, position.y - 20), text_color, buffer);
}

// Your existing functions (keeping them exactly as they are)
Vec3 esp::get_bone_position(uintptr_t ped, int bone_position)
{
    Matrix bone_matrix = mem.Read<Matrix>(ped + 0x60);
    Vector3 Head = mem.Read<Vector3>(ped + (0x410 + 0x10 * bone_position));
    DirectX::SimpleMath::Vector3 boneVec(Head.x, Head.y, Head.z);
    DirectX::SimpleMath::Vector3 transformedBoneVec = DirectX::XMVector3Transform(boneVec, bone_matrix);
    return Vec3(transformedBoneVec.x, transformedBoneVec.y, transformedBoneVec.z);
}

Vec3 esp::find_closest_player(Vec3& localPlayerPosition, Matrix viewmatrix) {
    float minDistance = 1000.0f;
    Vec3 closestPedPosition = Vec3(1.0f, 1.0f, 1.0f);

    // Get all valid ped IDs
    std::vector<uintptr_t> validPedIds = g_pedCacheManager.getValidPedIds();

    for (uintptr_t pedPointer : validPedIds) {
        PedData data;
        if (g_pedCacheManager.getPedData(pedPointer, data)) {
            float dx = localPlayerPosition.x - data.position_origin.x;
            float dy = localPlayerPosition.y - data.position_origin.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                closestPedPosition = data.position_origin;
            }
        }
    }

    return closestPedPosition;
}

// Configuration functions
void esp::set_skeleton_color(ImU32 color) { skeleton_color = color; }
void esp::set_line_thickness(float thickness) { line_thickness = thickness; }
void esp::set_use_batch_skeleton(bool use_batch) { use_batch_skeleton = use_batch; }

void esp::set_draw_peds(bool draw) { draw_peds = draw; }
bool esp::get_draw_peds() { return draw_peds; }

void esp::set_skeleton_enabled(bool enabled) { skeleton_enabled = enabled; }
bool esp::get_skeleton_enabled() { return skeleton_enabled; }

ImU32 esp::get_skeleton_color() { return skeleton_color; }
float esp::get_line_thickness() { return line_thickness; }
bool esp::get_use_batch_skeleton() { return use_batch_skeleton; }

// Box ESP flag default
bool esp::draw_box = false;

// Snapline defaults
bool esp::draw_snaplines = false;
esp::SnaplinePosition esp::snapline_position = esp::SnaplinePosition::CENTER;

// Draw dead peds toggle default: true
bool esp::draw_dead = false;

// Dead skeleton color default (gray)
ImU32 esp::dead_skeleton_color = IM_COL32(100, 100, 100, 255);

void esp::set_draw_dead(bool enabled) { draw_dead = enabled; }
bool esp::get_draw_dead() { return draw_dead; }

void esp::set_dead_skeleton_color(ImU32 color) { dead_skeleton_color = color; }
ImU32 esp::get_dead_skeleton_color() { return dead_skeleton_color; }

void esp::set_draw_snaplines(bool enabled) { draw_snaplines = enabled; }
bool esp::get_draw_snaplines() { return draw_snaplines; }

void esp::set_snapline_position(SnaplinePosition pos) { snapline_position = pos; }
esp::SnaplinePosition esp::get_snapline_position() { return snapline_position; }

// Render distance default (meters)
float esp::render_distance = 300.0f;

void esp::set_render_distance(float dist) { render_distance = dist; }
float esp::get_render_distance() { return render_distance; }

// Draw local player default: disabled
bool esp::draw_local_player = false;

void esp::set_draw_local_player(bool enabled) { draw_local_player = enabled; }
bool esp::get_draw_local_player() { return draw_local_player; }

// Name rendering removed from production build to improve performance and reduce string/memory reads.

// Box ESP functions (declarations implemented)
void esp::set_draw_box(bool enabled) { draw_box = enabled; }
bool esp::get_draw_box() { return draw_box; }

// Box color default (yellow-ish)
ImU32 esp::box_color = IM_COL32(255, 255, 0, 255);

void esp::set_box_color(ImU32 color) { box_color = color; }
ImU32 esp::get_box_color() { return box_color; }

// Draw a simple 2D axis-aligned box around ped using available bone/screen positions.
// This is a conservative box computed from head and hip/world positions and projected to screen.
void esp::draw_box_for_ped_cached(uintptr_t ped, Matrix viewport, uintptr_t localplayer, const PedData& cached_ped_data) {
    if (!get_draw_box()) return;

    // Try to use enhanced bone cache for accurate box extents
    std::vector<Vec3> bone_positions;
    if (!get_skeleton_bones_for_ped(ped, bone_positions, false)) return;

    // Compute AABB in world space from available bones
    Vec3 minv(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxv(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& b : bone_positions) {
        if (b.x == 0 && b.y == 0 && b.z == 0) continue; // skip invalid
        if (b.x < minv.x) minv.x = b.x;
        if (b.y < minv.y) minv.y = b.y;
        if (b.z < minv.z) minv.z = b.z;
        if (b.x > maxv.x) maxv.x = b.x;
        if (b.y > maxv.y) maxv.y = b.y;
        if (b.z > maxv.z) maxv.z = b.z;
    }

    // If still invalid, abort
    if (minv.x == FLT_MAX) return;

    // Project 8 corners and compute screen AABB
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    Vec3 corners[8] = {
        { minv.x, minv.y, minv.z }, { minv.x, minv.y, maxv.z }, { minv.x, maxv.y, minv.z }, { minv.x, maxv.y, maxv.z },
        { maxv.x, minv.y, minv.z }, { maxv.x, minv.y, maxv.z }, { maxv.x, maxv.y, minv.z }, { maxv.x, maxv.y, maxv.z }
    };

    bool anyOnScreen = false;
    Vec2 proj[8];
    float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;

    for (int i = 0; i < 8; ++i) {
        Vec2 s;
        if (!corners[i].world_to_screen(viewport, s)) continue;
        anyOnScreen = true;
        proj[i] = s;
        if (s.x < left) left = s.x;
        if (s.y < top) top = s.y;
        if (s.x > right) right = s.x;
        if (s.y > bottom) bottom = s.y;
    }

    if (!anyOnScreen) return;

    // Apply padding based on distance to localplayer for readability
    Vec3 local_pos = mem.Read<Vec3>(localplayer + 0x90);
    float distance = local_pos.distance_to((minv + maxv) * 0.5f);
    // Avoid Windows "max" macro collisions by using fmaxf and manual clamp for denominator
    float denom = distance < 1.0f ? 1.0f : distance;
    float pad = fmaxf(2.0f, 100.0f / denom);

    ImU32 col = esp::get_box_color();
    draw_list->AddRect(ImVec2(left - pad, top - pad), ImVec2(right + pad, bottom + pad), col, 0.0f, 0, esp::get_line_thickness());
}

void esp::draw_box_for_ped(uintptr_t ped, Matrix viewport, uintptr_t localplayer) {
    if (!get_draw_box()) return;

    // Try to get cached ped data then call cached version
    PedData cached;
    if (g_pedCacheManager.getPedData(ped, cached)) {
        draw_box_for_ped_cached(ped, viewport, localplayer, cached);
        return;
    }

    // Fallback: try to use bone positions directly
    std::vector<Vec3> bone_positions;
    if (!get_skeleton_bones_for_ped(ped, bone_positions, true)) return;

    // Build a temporary PedData for the cached function (only health/pos used elsewhere)
    PedData tmp; tmp.position_origin = bone_positions[0]; tmp.health = mem.Read<float>(ped + 0x280);
    draw_box_for_ped_cached(ped, viewport, localplayer, tmp);
}

// Snapline helper: compute start point based on configured position
static ImVec2 GetSnaplineStartPoint(esp::SnaplinePosition pos) {
    float sx = (float)width * 0.5f;
    if (pos == esp::SnaplinePosition::CENTER) {
        return ImVec2(sx, (float)height * 0.5f);
    }
    else if (pos == esp::SnaplinePosition::TOP) {
        return ImVec2(sx, 0.0f);
    }
    else { // BOTTOM
        return ImVec2(sx, (float)height);
    }
}

void esp::draw_snapline_for_ped(uintptr_t ped, Matrix viewport, uintptr_t localplayer, const PedData* cached) {
    if (!get_draw_snaplines()) return;

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Determine target world position: head if available else origin
    Vec3 worldTarget;
    bool haveTarget = false;

    if (cached) {
        // try head from cache if possible
        std::vector<Vec3> bones;
        if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 0) {
            worldTarget = bones[0];
            haveTarget = true;
        }
        if (!haveTarget) {
            worldTarget = cached->position_origin;
            haveTarget = true;
        }
    }
    else {
        // try to read bone positions directly
        std::vector<Vec3> bones;
        if (get_skeleton_bones_for_ped(ped, bones, true) && bones.size() > 0) {
            worldTarget = bones[0];
            haveTarget = true;
        }
        if (!haveTarget) {
            // fallback to ped root position
            worldTarget = mem.Read<Vec3>(ped + FiveM::offset::playerPosition);
            haveTarget = true;
        }
    }

    if (!haveTarget) return;

    // Project to screen
    Vec2 screenPos;
    if (!worldTarget.world_to_screen(viewport, screenPos)) return;

    // Draw from configured start to target
    ImVec2 start = GetSnaplineStartPoint(snapline_position);
    ImVec2 end(screenPos.x, screenPos.y);

    // Use configured snapline color (user selectable)
    ImU32 col = esp::snapline_color;
    draw_list->AddLine(start, end, col, 1.0f);
}

// Batch update and other functions (keeping your existing implementations)
void esp::batch_update_bone_cache(const std::vector<uintptr_t>& peds) {
    if (peds.empty()) return;

    auto handle = mem.CreateScatterHandle();
    std::vector<Matrix> bone_matrices(peds.size());
    std::vector<Vector3> head_offsets(peds.size());

    for (size_t i = 0; i < peds.size(); ++i) {
        mem.AddScatterReadRequest(handle, peds[i] + BONE_MATRIX_OFFSET,
            &bone_matrices[i], sizeof(Matrix));
        mem.AddScatterReadRequest(handle, peds[i] + (BONE_ARRAY_BASE + BONE_SIZE * 0),
            &head_offsets[i], sizeof(Vector3));
    }

    mem.ExecuteReadScatter(handle);
    mem.CloseScatterHandle(handle);

    for (size_t i = 0; i < peds.size(); ++i) {
        DirectX::SimpleMath::Vector3 boneVec(head_offsets[i].x, head_offsets[i].y, head_offsets[i].z);
        DirectX::SimpleMath::Vector3 transformedBoneVec = DirectX::XMVector3Transform(boneVec, bone_matrices[i]);
        Vec3 head_pos(transformedBoneVec.x, transformedBoneVec.y, transformedBoneVec.z);

        bone_cache.update_bone_data(peds[i], bone_matrices[i], head_pos);
    }

    esp_stats.memory_reads += peds.size() * 2;
}

// Batch skeleton update for multiple peds (performance optimization)
void esp::batch_update_skeleton_cache(const std::vector<uintptr_t>& peds) {
    if (peds.empty()) return;

    // Batch read all skeleton data
    for (uintptr_t ped : peds) {
        std::vector<Vec3> bone_positions;
        enhanced_bone_cache.get_skeleton_bones(ped, bone_positions, false); // Use cache when possible
    }
}

Vec3 esp::get_bone_position_cached(uintptr_t ped, int bone_position, const PedData& cached_ped_data) {
    if (bone_position == 0 && bone_cache.is_data_valid(ped)) {
        return bone_cache.get_bone_position(ped, 0, false);
    }
    return bone_cache.get_bone_position(ped, bone_position, true);
}

void esp::print_esp_stats() {
    static auto last_print = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (now - last_print >= std::chrono::seconds(5)) {
        double hit_ratio = esp_stats.get_hit_ratio();
        size_t cache_size = bone_cache.get_cache_size();
        size_t skeleton_cache_size = enhanced_bone_cache.get_skeleton_cache_size();

        std::cout << "[ESP Cache] Hit Ratio: " << (hit_ratio * 100.0) << "%"
            << ", Head Cache: " << cache_size
            << ", Skeleton Cache: " << skeleton_cache_size
            << ", Memory Reads: " << esp_stats.memory_reads
            << ", Batch Reads: " << esp_stats.batch_reads << std::endl;

        esp_stats.reset();
        last_print = now;
    }
}

void esp::DrawDebugLine(const Vec2& from, const Vec2& to, ImU32 color) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), color, 1.0f);
}

void esp::DrawDebugText(const Vec2& pos, const char* text, ImU32 color) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddText(ImVec2(pos.x, pos.y), color, text);
}

void esp::DrawDebugCircle(const Vec2& center, float radius, ImU32 color) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddCircle(ImVec2(center.x, center.y), radius, color, 12, 1.0f);
}

// Cache management functions
size_t esp::get_skeleton_cache_size() {
    return enhanced_bone_cache.get_skeleton_cache_size();
}

void esp::clear_skeleton_cache() {
    enhanced_bone_cache.clear_skeleton_cache();
}

void esp::set_use_cache(bool use) { use_cache = use; }
bool esp::get_use_cache() { return use_cache; }

// Distance ESP defaults
bool esp::distance_enabled = false;
ImU32 esp::distance_color = IM_COL32(255, 255, 255, 255);
esp::NamePosition esp::distance_position = esp::NamePosition::BOTTOM;

// Distance caches
std::unordered_map<uintptr_t, int> esp::distance_meters_cache;
std::unordered_map<uintptr_t, float> esp::distance_sq_cache;

void esp::set_distance_enabled(bool enabled) { distance_enabled = enabled; }
bool esp::get_distance_enabled() { return distance_enabled; }
void esp::set_distance_color(ImU32 color) { distance_color = color; }
ImU32 esp::get_distance_color() { return distance_color; }
// distance_text_size and distance_max_render_distance removed; rendering uses esp::get_render_distance()
void esp::set_distance_position(NamePosition pos) { distance_position = pos; }
esp::NamePosition esp::get_distance_position() { return distance_position; }

// Drawing helper - read-only, uses cached values only
void esp::draw_distance_for_ped(uintptr_t ped, Matrix viewport, const PedData* cached) {
    if (!distance_enabled) return;

    auto it_sq = distance_sq_cache.find(ped);
    if (it_sq == distance_sq_cache.end()) return; // no cached distance

    float dist_sq = it_sq->second;
    float maxd = esp::get_render_distance();
    if (dist_sq > maxd * maxd) return;

    // need meters value
    int meters = 0;
    auto it_m = distance_meters_cache.find(ped);
    if (it_m != distance_meters_cache.end()) meters = it_m->second;
    else {
        // fallback compute from sq (avoid sqrt except here)
        meters = (int)std::floor(std::sqrt(dist_sq));
    }

    // Projected box for alignment - try to compute a screen anchor using existing functions
    // Use compute_screen_aabb helper in this file (not exported) - recreate minimal logic: try to get AABB
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    // Determine text and position using cached PedData only (no DMA reads)
    char buf[32];
    snprintf(buf, sizeof(buf), "%dm", meters);
    ImVec2 text_size = ImGui::CalcTextSize(buf);

    ImVec2 pos;
    const float padding = 4.0f;

    if (cached) {
        // project cached world position
        Vec2 screen;
        if (!cached->position_origin.world_to_screen(viewport, screen)) return;
        // Offsets and sizes (match health/armor drawing assumptions)
        const float HOR_BAR_H = 4.0f; // horizontal bar height
        const float BAR_PADDING = 2.0f; // spacing between bars
        const float BASE_OFFSET = 6.0f; // base spacing from anchor
        const float SIDE_BASE_HEALTH = 12.0f; // used in health bar code
        const float SIDE_BASE_ARMOR = 18.0f;  // used in armor bar code

        // Compute cumulative offsets based on which bars are enabled and their positions
        float top_offset = BASE_OFFSET;
        float bottom_offset = BASE_OFFSET;
        float left_offset = 0.0f;
        float right_offset = 0.0f;

        if (esp::health_bar_enabled) {
            if (esp::health_bar_position == NamePosition::TOP) {
                top_offset += HOR_BAR_H + BAR_PADDING;
            }
            else if (esp::health_bar_position == NamePosition::BOTTOM) {
                bottom_offset += HOR_BAR_H + BAR_PADDING;
            }
            else if (esp::health_bar_position == NamePosition::LEFT) {
                left_offset += SIDE_BASE_HEALTH;
            }
            else if (esp::health_bar_position == NamePosition::RIGHT) {
                right_offset += SIDE_BASE_HEALTH;
            }
        }

        if (esp::armor_bar_enabled) {
            if (esp::armor_bar_position == NamePosition::TOP) {
                top_offset += HOR_BAR_H + BAR_PADDING;
            }
            else if (esp::armor_bar_position == NamePosition::BOTTOM) {
                bottom_offset += HOR_BAR_H + BAR_PADDING;
            }
            else if (esp::armor_bar_position == NamePosition::LEFT) {
                left_offset += SIDE_BASE_ARMOR;
            }
            else if (esp::armor_bar_position == NamePosition::RIGHT) {
                right_offset += SIDE_BASE_ARMOR;
            }
        }

        // If very close, force distance under feet for readability
        const int CLOSE_FEET_THRESHOLD = 10; // meters
        if (meters <= CLOSE_FEET_THRESHOLD) {
            std::vector<Vec3> bones;
            if (get_skeleton_bones_for_ped(ped, bones, false) && !bones.empty()) {
                bool found = false;
                Vec2 best_screen;
                float best_y = -1e9f;
                for (size_t bi = 0; bi < bones.size(); ++bi) {
                    Vec2 s;
                    if (!bones[bi].world_to_screen(viewport, s)) continue;
                    if (!found || s.y > best_y) {
                        best_y = s.y;
                        best_screen = s;
                        found = true;
                    }
                }
                if (found) {
                    pos.x = best_screen.x - text_size.x * 0.5f;
                    pos.y = best_screen.y + bottom_offset + 4.0f; // small gap under feet
                    // Render under feet even if position setting differs
                    ImU32 col = distance_color;
                    draw_list->AddText(ImVec2(pos.x, pos.y), col, buf);
                    return;
                }
            }
        }

        if (distance_position == NamePosition::TOP) {
            // Try to place above head if skeleton bones cached
            std::vector<Vec3> bones;
            if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 0) {
                Vec3 head = bones[0];
                Vec2 head_s;
                if (head.world_to_screen(viewport, head_s)) {
                    pos.x = head_s.x - text_size.x * 0.5f;
                    pos.y = head_s.y - top_offset - text_size.y - 4.0f; // small gap above head
                }
                else {
                    pos.x = screen.x - text_size.x * 0.5f;
                    pos.y = screen.y - top_offset - text_size.y;
                }
            }
            else {
                pos.x = screen.x - text_size.x * 0.5f;
                pos.y = screen.y - top_offset - text_size.y;
            }
        }
        else if (distance_position == NamePosition::BOTTOM) {
            // Place under stomach/hip bone when far; if close, earlier block already handled feet
            std::vector<Vec3> bones;
            if (get_skeleton_bones_for_ped(ped, bones, false) && bones.size() > 6) {
                Vec3 stomach = bones[6];
                Vec2 st_s;
                if (stomach.world_to_screen(viewport, st_s)) {
                    pos.x = st_s.x - text_size.x * 0.5f;
                    pos.y = st_s.y + bottom_offset + 4.0f;
                }
                else {
                    pos.x = screen.x - text_size.x * 0.5f;
                    pos.y = screen.y + bottom_offset;
                }
            }
            else {
                pos.x = screen.x - text_size.x * 0.5f;
                pos.y = screen.y + bottom_offset;
            }
        }
        else if (distance_position == NamePosition::LEFT) {
            pos.x = screen.x - left_offset - text_size.x - BASE_OFFSET;
            pos.y = screen.y - text_size.y * 0.5f;
        }
        else { // RIGHT
            pos.x = screen.x + right_offset + BASE_OFFSET;
            pos.y = screen.y - text_size.y * 0.5f;
        }
    }
    else {
        // No cached position available; skip rendering to avoid DMA reads in render path
        return;
    }

    // Render text
    ImU32 col = distance_color;
    draw_list->AddText(ImVec2(pos.x, pos.y), col, buf);
}

