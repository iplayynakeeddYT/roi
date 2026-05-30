#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "makcu.h"

namespace makcu_wrapper {
    // Global state
    extern std::unique_ptr<makcu::Device> device;
    extern std::atomic<bool> connected;

    // Initialize MAKCU device
    void MakcuInitialize(const std::string& port = "");

    // Mouse movement
    void move(int x, int y);

    // Mouse clicks
    void left_click();
    void left_click_release();

    // Key/button detection (maps to MAKCU mouse buttons)
    bool IsDown(int virtual_key);
    bool IsKeyJustPressed(int virtual_key);
    bool IsKeyJustReleased(int virtual_key);

    // Helper functions for mouse button management
    std::vector<std::string> GetAvailableMouseButtons();
    int GetMouseButtonKeyCode(const std::string& button_name);

    // Connection status
    bool IsConnected();
    std::string GetConnectionStatus();
}