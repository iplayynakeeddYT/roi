#include "makcu_wrapper.h"
#include <iostream>
#include <map>
#include <chrono>

namespace makcu_wrapper {
    // Global device instance
    std::unique_ptr<makcu::Device> device;
    std::atomic<bool> connected(false);

    // Previous button states for edge detection
    static std::map<int, bool> previous_states;
    static std::map<int, std::chrono::steady_clock::time_point> last_check_time;

    void MakcuInitialize(const std::string& port) {
        connected = false;
        
        if (!device) {
            device = std::make_unique<makcu::Device>();
        }

        // If already connected, disconnect first
        if (device->isConnected()) {
            device->disconnect();
        }

        std::cout << "Connecting to MAKCU device..." << std::endl;

        // Find and connect to device
        std::string target_port = port;
        if (target_port.empty()) {
            target_port = makcu::Device::findFirstDevice();
            if (target_port.empty()) {
                std::cerr << "No MAKCU device found!" << std::endl;
                return;
            }
        }

        if (device->connect(target_port)) {
            connected = true;
            
            // Enable high performance mode for gaming
            device->enableHighPerformanceMode(true);
            
            // Enable button monitoring
            device->enableButtonMonitoring(true);
            
            std::cout << "MAKCU device connected on port " << target_port << std::endl;
            
            // Get device version
            std::string version = device->getVersion();
            if (!version.empty()) {
                std::cout << "MAKCU firmware version: " << version << std::endl;
            }
        } else {
            std::cerr << "Failed to connect to MAKCU device!" << std::endl;
        }
    }

    void move(int x, int y) {
        if (!connected || !device || !device->isConnected()) {
            return;
        }

        // Manual clamp values to valid range
        if (x < -127) x = -127;
        if (x > 127) x = 127;
        if (y < -127) y = -127;
        if (y > 127) y = 127;

        // Use high-performance mouse move
        device->mouseMove(x, y);
    }

    void left_click() {
        if (!connected || !device || !device->isConnected()) {
            return;
        }
        device->mouseDown(makcu::MouseButton::LEFT);
    }

    void left_click_release() {
        if (!connected || !device || !device->isConnected()) {
            return;
        }
        device->mouseUp(makcu::MouseButton::LEFT);
    }

    // Map virtual key codes to MAKCU mouse buttons
    makcu::MouseButton VirtualKeyToMouseButton(int virtual_key) {
        switch (virtual_key) {
            case 1: // VK_LBUTTON
                return makcu::MouseButton::LEFT;
            case 2: // VK_RBUTTON
                return makcu::MouseButton::RIGHT;
            case 4: // VK_MBUTTON
                return makcu::MouseButton::MIDDLE;
            case 5: // VK_XBUTTON1
                return makcu::MouseButton::SIDE1;
            case 6: // VK_XBUTTON2
                return makcu::MouseButton::SIDE2;
            default:
                return makcu::MouseButton::LEFT;
        }
    }

    bool IsDown(int virtual_key) {
        if (!connected || !device || !device->isConnected()) {
            return false;
        }

        // Rate limiting to prevent excessive queries
        auto now = std::chrono::steady_clock::now();
        auto& last_check = last_check_time[virtual_key];
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check);
        
        // Only check every 10ms maximum
        if (elapsed.count() < 10) {
            return previous_states[virtual_key];
        }
        
        last_check = now;

        makcu::MouseButton button = VirtualKeyToMouseButton(virtual_key);
        bool state = device->mouseButtonState(button);
        previous_states[virtual_key] = state;
        
        return state;
    }

    bool IsKeyJustPressed(int virtual_key) {
        bool current_state = IsDown(virtual_key);
        bool& previous_state = previous_states[virtual_key];
        
        bool just_pressed = current_state && !previous_state;
        previous_state = current_state;
        
        return just_pressed;
    }

    bool IsKeyJustReleased(int virtual_key) {
        bool current_state = IsDown(virtual_key);
        bool& previous_state = previous_states[virtual_key];
        
        bool just_released = !current_state && previous_state;
        previous_state = current_state;
        
        return just_released;
    }

    std::vector<std::string> GetAvailableMouseButtons() {
        return {
            "Left Mouse",
            "Right Mouse",
            "Middle Mouse",
            "Mouse 4",
            "Mouse 5"
        };
    }

    int GetMouseButtonKeyCode(const std::string& button_name) {
        if (button_name == "Left Mouse") return 1;
        if (button_name == "Right Mouse") return 2;
        if (button_name == "Middle Mouse") return 4;
        if (button_name == "Mouse 4") return 5;
        if (button_name == "Mouse 5") return 6;
        return 2; // Default to right mouse
    }

    bool IsConnected() {
        return connected && device && device->isConnected();
    }

    std::string GetConnectionStatus() {
        if (!device) {
            return "No device instance";
        }
        
        switch (device->getStatus()) {
            case makcu::ConnectionStatus::CONNECTED:
                return "Connected";
            case makcu::ConnectionStatus::CONNECTING:
                return "Connecting...";
            case makcu::ConnectionStatus::DISCONNECTED:
                return "Disconnected";
            case makcu::ConnectionStatus::CONNECTION_ERROR:
                return "Connection Error";
            default:
                return "Unknown";
        }
    }
}