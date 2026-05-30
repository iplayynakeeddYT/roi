#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <setupapi.h>
#include <devguid.h>
#pragma comment(lib, "setupapi.lib")

namespace kmbox
{
	extern bool connected;
	extern std::string find_port(const std::string& targetDescription);
	extern void KmboxInitialize(std::string port);

	// Mouse movement and clicking
	extern void move(int x, int y);
	extern void left_click();
	extern void left_click_release();

	// FIXED: Key detection functions
	extern bool IsDown(int virtual_key);  // Now actually works!
	extern bool IsKeyJustPressed(int virtual_key);    // NEW: Just pressed detection
	extern bool IsKeyJustReleased(int virtual_key);   // NEW: Just released detection

	// NEW: Helper functions for mouse button management
	extern std::vector<std::string> GetAvailableMouseButtons();
	extern int GetMouseButtonKeyCode(const std::string& button_name);
}