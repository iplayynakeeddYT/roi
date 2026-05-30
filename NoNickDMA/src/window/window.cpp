#include "window.hpp"
#include <dwmapi.h>
#include <stdio.h>
#include <string>
#include <chrono>
#include "../game/game.h"  // This now includes all refactored headers
#include "../esp/esp.h"     // Add this to access esp namespace
#include "../playerInfo/PedData.h"  // Add this to access g_pedCacheManager
#include <iostream>
#include "../aimbot/aimbot.h"
#include "../makcu/makcu_wrapper.h"

// declaration of the ImGui_ImplWin32_WndProcHandler function
// basically integrates ImGui with the Windows message loop so ImGui can process input and events
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool enableESP = true; // checkbox state
static bool enableAimbot = false; // checkbox state

// ESP keybind system
static int g_espToggleKey = VK_F2; // Default key
static bool g_espTogglePrevDown = false;
static bool g_espCaptureMode = false; // Waiting for user to press a key to assign

// Helper: convert virtual-key to readable name
static std::string GetVirtualKeyName(int vk) {
	if (vk >= 'A' && vk <= 'Z') {
		char c = (char)vk;
		return std::string(1, c);
	}
	if (vk >= '0' && vk <= '9') {
		char c = (char)vk;
		return std::string(1, c);
	}
	switch (vk) {
	case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3"; case VK_F4: return "F4";
	case VK_F5: return "F5"; case VK_F6: return "F6"; case VK_F7: return "F7"; case VK_F8: return "F8";
	case VK_F9: return "F9"; case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
	case VK_SPACE: return "Space";
	case VK_RETURN: return "Enter";
	case VK_BACK: return "Backspace";
	case VK_TAB: return "Tab";
	case VK_ESCAPE: return "Esc";
	case VK_INSERT: return "Insert";
	case VK_DELETE: return "Delete";
	case VK_HOME: return "Home";
	case VK_END: return "End";
	case VK_PRIOR: return "PageUp";
	case VK_NEXT: return "PageDown";
	case VK_UP: return "Up";
	case VK_DOWN: return "Down";
	case VK_LEFT: return "Left";
	case VK_RIGHT: return "Right";
	case VK_LSHIFT: case VK_RSHIFT: return "Shift";
	case VK_LCONTROL: case VK_RCONTROL: return "Ctrl";
	case VK_LMENU: case VK_RMENU: return "Alt";
	default: {
		// Fallback try to format VK_#
		char buf[16];
		snprintf(buf, sizeof(buf), "VK_%02X", vk);
		return std::string(buf);
	}
	}
}

LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// set up ImGui window procedure handler
	if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam))
		return true;

	// switch that disables alt application and checks for if the user tries to close the window.
	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu (imgui uses it in their example :shrug:)
			return 0;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_CLOSE:
		return 0;
	}

	// define the window procedure
	return DefWindowProc(window, msg, wParam, lParam);
}

float Overlay::GetRefreshRate()
{
	// get dxgi variables
	IDXGIFactory* dxgiFactory = nullptr;
	IDXGIAdapter* dxgiAdapter = nullptr;
	IDXGIOutput* dxgiOutput = nullptr;
	DXGI_MODE_DESC modeDesc;

	// create DXGI factory
	if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&dxgiFactory))))
		return 60;

	// get the adapter (aka GPU)
	if (FAILED(dxgiFactory->EnumAdapters(0, &dxgiAdapter))) {
		dxgiAdapter->Release();
		return 60;
	}

	// get the MAIN monitor - to add multiple, you should use "dxgiAdapter->EnumOutputs" to loop through each monitor, and save it.
	// then, you can access the refresh rate of each one and save the highest one, then set the refreshrate to that.
	// i haven't had any issues just using the main one though.
	if (FAILED(dxgiAdapter->EnumOutputs(0, &dxgiOutput))) {
		dxgiAdapter->Release();
		dxgiFactory->Release();
		return 60;
	}

	// iterate through display modes
	UINT numModes = 0;
	if (FAILED(dxgiOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, nullptr))) {
		dxgiOutput->Release();
		dxgiAdapter->Release();
		dxgiFactory->Release();
		return 60;
	}

	DXGI_MODE_DESC* displayModeList = new DXGI_MODE_DESC[numModes];
	if (FAILED(dxgiOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, displayModeList))) {
		delete[] displayModeList;
		dxgiOutput->Release();
		dxgiAdapter->Release();
		dxgiFactory->Release();
		return 60;
	}

	float refreshRate = 60;
	// next, find the refresh rate
	for (int i = 0; i < numModes; ++i) {
		// check 
		float hz = static_cast<float>(displayModeList[i].RefreshRate.Numerator) /
			static_cast<float>(displayModeList[i].RefreshRate.Denominator);

		// make sure hz didn't return 0, and is more or the same.
		if (hz != 0 && hz >= refreshRate)
			refreshRate = hz;
	}

	delete[] displayModeList;
	dxgiOutput->Release();
	dxgiAdapter->Release();
	dxgiFactory->Release();

	printf("[>>] Refresh rate: %f", refreshRate);
	printf("\n"); // i genuinely do not care anymore

	return refreshRate;
}

bool Overlay::CreateDevice()
{
	// First we setup our swap chain, this basically just holds a bunch of descriptors for the swap chain.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));

	// set number of back buffers (this is double buffering)
	sd.BufferCount = 2;

	// width + height of buffer, (0 is automatic sizing)
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;

	// set the pixel format
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// get the fps from GetRefreshRate(). If anything fails it just returns 60 anyways.
	sd.BufferDesc.RefreshRate.Numerator = GetRefreshRate();
	sd.BufferDesc.RefreshRate.Denominator = 1;

	// allow mode switch (changing display modes)
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// set how the bbuffer will be used
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	sd.OutputWindow = overlay;

	// setup the multi-sampling
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;

	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// specify what Direct3D feature levels to use
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	// create device and swap chain
	HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&featureLevel,
		&device_context);

	// if the hardware isn't supported create with WARP (basically just a different renderer)
	if (result == DXGI_ERROR_UNSUPPORTED) {
		result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0U,
			featureLevelArray,
			2, D3D11_SDK_VERSION,
			&sd,
			&swap_chain,
			&device,
			&featureLevel,
			&device_context);

		printf("[>>] DXGI_ERROR | Created with D3D_DRIVER_TYPE_WARP\n");
	}

	// can't do much more, if the hardware still isn't supported just return false.
	if (result != S_OK) {
		printf("[>>] Device Not Okay\n");
		return false;
	}

	// retrieve back_buffer, im defining it here since it isn't being used at any other point in time.
	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	// if back buffer is obtained then we can create render target view and release the back buffer again
	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_targetview);
		back_buffer->Release();

		printf("[>>] Created Device\n");
		return true;
	}

	// if we reach this point then it failed to create the back buffer
	printf("[>>] Failed to create Device\n");
	return false;
}

void Overlay::DestroyDevice()
{
	// release everything that has to do with the device.
	if (device)
	{
		device->Release();
		device_context->Release();
		swap_chain->Release();
		render_targetview->Release();

		printf("[>>] Released Device\n");
	}
	else
		printf("[>>] Device Not Found when Exiting.\n");
}

void Overlay::CreateOverlay(const char* window_name)
{
	// Convert const char* to std::wstring (Unicode)
	int len = MultiByteToWideChar(CP_UTF8, 0, window_name, -1, NULL, 0);
	std::wstring wide_window_name(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, window_name, -1, &wide_window_name[0], len);

	// Define window class
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"overlay";

	// Register class
	RegisterClassExW(&wc);

	// Create window
	// Removed WS_EX_TRANSPARENT and WS_EX_LAYERED
	// We are no longer aiming for a transparent click-through window initially.
	overlay = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		wc.lpszClassName,
		wide_window_name.c_str(),
		WS_POPUP, // WS_POPUP is generally fine
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	if (overlay == NULL)
		printf("[>>] Failed to create Overlay\n");

	// set overlay window attributes to make the overlay transparent
	//SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	// set up the DWM frame extension for client area
	{
		// first we define our RECT structures that hold our client and window area
		RECT client_area{};
		RECT window_area{};

		// get the client and window area
		GetClientRect(overlay, &client_area);
		GetWindowRect(overlay, &window_area);

		// calculate the difference between the screen and window coordinates
		POINT diff{};
		ClientToScreen(overlay, &diff);

		// calculate the margins for DWM frame extension
		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		// then we extend the frame into the client area
		DwmExtendFrameIntoClientArea(overlay, &margins);
	}

	// show + update overlay
	ShowWindow(overlay, SW_SHOW);
	UpdateWindow(overlay);

	printf("[>>] Overlay Created\n");
}

void Overlay::DestroyOverlay()
{
	DestroyWindow(overlay);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
}

bool Overlay::CreateImGui()
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Initalize ImGui for the Win32 library
	if (!ImGui_ImplWin32_Init(overlay)) {
		printf("[>>] Failed ImGui_ImplWin32_Init\n");
		return false;
	}

	// Initalize ImGui for DirectX 11.
	if (!ImGui_ImplDX11_Init(device, device_context)) {
		printf("[>>] Failed ImGui_ImplDX11_Init\n");
		return false;
	}

	printf("[>>] ImGui Initialized\n");
	return true;
}

void Overlay::DestroyImGui()
{
	// Cleanup ImGui by shutting down DirectX11, the Win32 Platform and Destroying the ImGui context.
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}



void Overlay::StartRender()
{
	// handle windows messages
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// FPS Counter variables (static to persist between calls)
	static auto last_time = std::chrono::high_resolution_clock::now();
	static int frame_count = 0;
	static float fps = 0.0f;

	// Calculate FPS
	frame_count++;
	auto current_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);

	// Update FPS every 100ms for smooth display
	if (duration.count() >= 100) {
		fps = frame_count * 1000.0f / duration.count();
		frame_count = 0;
		last_time = current_time;
	}

	// begin a new frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Only clear bone cache if ESP cache is disabled (forces fresh reads)
	// When cache is enabled, we want to keep the cached data
	if (!esp::get_use_cache()) {
		esp::bone_cache.clear();
	}

	// FPS Counter Window
	ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 120, 60 }, ImGuiCond_FirstUseEver);
	ImGui::Begin("FPS Counter", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("FPS: %.1f", fps);
	ImGui::End();

	// Enhanced key handling for features - DECLARE ALL STATIC VARIABLES HERE
	static bool f1_pressed = false, f2_pressed = false, f3_pressed = false;
	static bool activation_button_pressed = false;


	// Run ESP if enabled
    // Poll global keybind even if menu is closed
	{
		// If in capture mode, block normal toggle handling and capture the next key
		if (g_espCaptureMode) {
			// Iterate possible keys and detect press
			for (int vk = 0x08; vk <= 0xFE; ++vk) {
				SHORT state = GetAsyncKeyState(vk);
				if (state & 0x8000) {
					// assign new key and exit capture mode
					g_espToggleKey = vk;
					g_espCaptureMode = false;
					// Reset previous down state to avoid instant toggle
					g_espTogglePrevDown = true;
					break;
				}
			}
		}
	else {
		// Normal operation: toggle ESP on key press with debounce
		SHORT state = GetAsyncKeyState(g_espToggleKey);
		bool isDown = (state & 0x8000) != 0;
		if (isDown && !g_espTogglePrevDown) {
			// Toggle
			enableESP = !enableESP;
		}
		g_espTogglePrevDown = isDown;
	}
	}

	if (enableESP) {
		FiveM::ESP::RunESP();
	}


	// Menu toggle handling
	if (GetAsyncKeyState(VK_INSERT) & 1) {
		RenderMenu = !RenderMenu;

		// Set window styles based on menu state
		if (RenderMenu) {
			// Menu visible - allow interaction
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
		}
		else {
			// Menu hidden - make transparent and click-through
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
		}
	}
}

void Overlay::EndRender()
{
	// Render ImGui
	ImGui::Render();

	float color[4]{ 0, 0, 0, 1 };

	device_context->OMSetRenderTargets(1, &render_targetview, nullptr);
	device_context->ClearRenderTargetView(render_targetview, color);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	swap_chain->Present(0U, 0U);
}

void Overlay::Render()
{
	// Main menu window
	ImGui::SetNextWindowSize({ 550, 550 });
	ImGui::Begin("NoNick DMA", &RenderMenu, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::BeginTabBar("tabBar1", ImGuiTabBarFlags_None))
	{

		// Aimbot Settings Tab
		if (ImGui::BeginTabItem("Aimbot")) {

			ImGui::EndTabItem();
		}

		// ESP Section
		if (ImGui::BeginTabItem("Visuals")) {
			ImGui::Checkbox("Enable ESP", &enableESP);

			// ESP Keybind
			{
				ImGui::SameLine();
				ImGui::Text("");
				ImGui::SameLine();
				char buf[64];
				snprintf(buf, sizeof(buf), "[ %s ]", GetVirtualKeyName(g_espToggleKey).c_str());
				if (ImGui::Button(buf, ImVec2(80, 0))) {
					// Enter capture mode
					g_espCaptureMode = true;
				}
				if (g_espCaptureMode) {
					ImGui::SameLine(); ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "Press any key...");
				}
			}

			if (enableESP) {
				ImGui::Separator();

				// 1. Draw Peds Toggle
				{
					bool draw_peds = esp::get_draw_peds();
					if (ImGui::Checkbox("Draw Pedestrian", &draw_peds)) {
						esp::set_draw_peds(draw_peds);
					}
				}

                // 2. Skeleton Toggle (Now uses dedicated flag)
				{
					bool is_skeleton = esp::get_skeleton_enabled();

					if (ImGui::Checkbox("Draw Skeleton", &is_skeleton)) {
						esp::set_skeleton_enabled(is_skeleton);
					}

					// 3. Conditional Line Thickness (Only shows if Skeleton is ON)
					if (esp::get_skeleton_enabled()) {
						ImGui::Indent(20.0f); // Slight indent to show it belongs to Skeleton

						// Skeleton color picker (only visible when skeleton is enabled)
						ImGui::SetNextItemWidth(150.0f);
						{
					ImU32 c = esp::skeleton_color;
					static float skeleton_col[4] = {1.0f, 0.0f, 0.0f, 1.0f};
					skeleton_col[0] = (float)((c      ) & 0xFF) / 255.0f;
					skeleton_col[1] = (float)((c >> 8 ) & 0xFF) / 255.0f;
					skeleton_col[2] = (float)((c >> 16) & 0xFF) / 255.0f;
					skeleton_col[3] = (float)((c >> 24) & 0xFF) / 255.0f;
					if (ImGui::ColorEdit4("Skeleton Color##skeleton_color", skeleton_col, ImGuiColorEditFlags_NoInputs)) {
						ImU32 nc = IM_COL32((int)(skeleton_col[0]*255.0f), (int)(skeleton_col[1]*255.0f), (int)(skeleton_col[2]*255.0f), (int)(skeleton_col[3]*255.0f));
						esp::set_skeleton_color(nc);
					}
						}

						float line_thickness = esp::get_line_thickness();
						ImGui::SetNextItemWidth(150.0f); // Keep slider from being too wide
						if (ImGui::SliderFloat("Line Thickness", &line_thickness, 1.0f, 5.0f, "%.1f")) {
							esp::set_line_thickness(line_thickness);
						}

						ImGui::Unindent(20.0f);
					}

				// Distance ESP
				{
					bool de = esp::get_distance_enabled();
					if (ImGui::Checkbox("DrawDistance", &de)) {
						esp::set_distance_enabled(de);
					}
					if (esp::get_distance_enabled()) {
						ImGui::Indent(20.0f);
						// Position combo (Top/Bottom/Left/Right) - reuse NamePosition enum
						int pos = (int)esp::get_distance_position();
						const char* pos_items[] = { "Top", "Bottom", "Left", "Right" };
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::Combo("Distance Position", &pos, pos_items, IM_ARRAYSIZE(pos_items))) {
							esp::set_distance_position((esp::NamePosition)pos);
						}

						// Color picker
						ImU32 dc = esp::get_distance_color();
						static float distance_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
						distance_col[0] = (float)((dc      ) & 0xFF) / 255.0f;
						distance_col[1] = (float)((dc >> 8 ) & 0xFF) / 255.0f;
						distance_col[2] = (float)((dc >> 16) & 0xFF) / 255.0f;
						distance_col[3] = (float)((dc >> 24) & 0xFF) / 255.0f;
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::ColorEdit4("Distance Color##distance_color", distance_col, ImGuiColorEditFlags_NoInputs)) {
							ImU32 nc = IM_COL32((int)(distance_col[0]*255.0f), (int)(distance_col[1]*255.0f), (int)(distance_col[2]*255.0f), (int)(distance_col[3]*255.0f));
							esp::set_distance_color(nc);
						}

						// Distance uses global Render Distance setting

						ImGui::Unindent(20.0f);
					}
				}

				// Draw Dead toggle and color
				{
					bool dd = esp::get_draw_dead();
					if (ImGui::Checkbox("Draw Dead", &dd)) {
						esp::set_draw_dead(dd);
					}
					if (esp::get_draw_dead()) {
						ImGui::Indent(20.0f);
						ImU32 dc = esp::get_dead_skeleton_color();
						static float dead_col[4] = {0.39f, 0.39f, 0.39f, 1.0f};
						dead_col[0] = (float)((dc      ) & 0xFF) / 255.0f;
						dead_col[1] = (float)((dc >> 8 ) & 0xFF) / 255.0f;
						dead_col[2] = (float)((dc >> 16) & 0xFF) / 255.0f;
						dead_col[3] = (float)((dc >> 24) & 0xFF) / 255.0f;
						if (ImGui::ColorEdit4("Dead Color##dead_skeleton_color", dead_col, ImGuiColorEditFlags_NoInputs)) {
							ImU32 nc = IM_COL32((int)(dead_col[0]*255.0f), (int)(dead_col[1]*255.0f), (int)(dead_col[2]*255.0f), (int)(dead_col[3]*255.0f));
							esp::set_dead_skeleton_color(nc);
						}
						ImGui::Unindent(20.0f);
					}
				}
				}

               // Box ESP Toggle
				{
					bool draw_box = esp::get_draw_box();
					if (ImGui::Checkbox("Draw Box", &draw_box)) {
						esp::set_draw_box(draw_box);
					}
					// Box color picker visible when box enabled
					if (esp::get_draw_box()) {
						ImGui::Indent(20.0f);
						ImGui::SetNextItemWidth(150.0f);
						ImU32 bc = esp::get_box_color();
					static float box_col[4] = {1.0f, 1.0f, 0.0f, 1.0f};
					box_col[0] = (float)((bc      ) & 0xFF) / 255.0f;
					box_col[1] = (float)((bc >> 8 ) & 0xFF) / 255.0f;
					box_col[2] = (float)((bc >> 16) & 0xFF) / 255.0f;
					box_col[3] = (float)((bc >> 24) & 0xFF) / 255.0f;
					if (ImGui::ColorEdit4("Box Color##box_color", box_col, ImGuiColorEditFlags_NoInputs)) {
						ImU32 nc = IM_COL32((int)(box_col[0]*255.0f), (int)(box_col[1]*255.0f), (int)(box_col[2]*255.0f), (int)(box_col[3]*255.0f));
						esp::set_box_color(nc);
					}
						ImGui::Unindent(20.0f);
					}
				}

              // Snapline Toggle
				{
					bool draw_snap = esp::get_draw_snaplines();
					if (ImGui::Checkbox("Draw Snaplines", &draw_snap)) {
						esp::set_draw_snaplines(draw_snap);
					}

					// Show position options only when snaplines are enabled
					if (esp::get_draw_snaplines()) {
						ImGui::Indent(20.0f);
						const char* items[] = { "Center", "Top", "Bottom" };
						int current = (int)esp::get_snapline_position();
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::Combo("Snapline Position", &current, items, IM_ARRAYSIZE(items))) {
							esp::set_snapline_position((esp::SnaplinePosition)current);
						}

						// Snapline color picker
						ImGui::SetNextItemWidth(150.0f);
						{
							ImU32 c = esp::snapline_color;
						static float snap_col[4] = {1.0f, 1.0f, 1.0f, 0.78f};
						snap_col[0] = (float)((c      ) & 0xFF) / 255.0f;
						snap_col[1] = (float)((c >> 8 ) & 0xFF) / 255.0f;
						snap_col[2] = (float)((c >> 16) & 0xFF) / 255.0f;
						snap_col[3] = (float)((c >> 24) & 0xFF) / 255.0f;
						if (ImGui::ColorEdit4("Snapline Color##snapline_color", snap_col, ImGuiColorEditFlags_NoInputs)) {
							esp::snapline_color = IM_COL32((int)(snap_col[0]*255.0f), (int)(snap_col[1]*255.0f), (int)(snap_col[2]*255.0f), (int)(snap_col[3]*255.0f));
						}
						}
						ImGui::Unindent(20.0f);
					}
				}

				// Render Distance slider
				{
					float dist = esp::get_render_distance();
					ImGui::Indent(20.0f);
					ImGui::SetNextItemWidth(300.0f);
                 ImGui::Text("Render Distance");
					if (ImGui::SliderFloat(" ", &dist, 0.0f, 500.0f, "%.0f m")) {
						esp::set_render_distance(dist);
					}
					ImGui::Unindent(20.0f);
				}

				// Draw Local Player toggle
				{
					bool draw_local = esp::get_draw_local_player();
					if (ImGui::Checkbox("Draw Local Player", &draw_local)) {
						esp::set_draw_local_player(draw_local);
					}
				}

				// Health Bar Toggle and options
				{
					bool hb = esp::health_bar_enabled;
					if (ImGui::Checkbox("Draw Health Bar", &hb)) {
						esp::health_bar_enabled = hb;
					}

					if (esp::health_bar_enabled) {
						ImGui::Indent(20.0f);
						{
							ImU32 c = esp::health_bar_color;
						static float health_col[4] = {0.0f, 1.0f, 0.0f, 1.0f};
						health_col[0] = (float)((c      ) & 0xFF) / 255.0f;
						health_col[1] = (float)((c >> 8 ) & 0xFF) / 255.0f;
						health_col[2] = (float)((c >> 16) & 0xFF) / 255.0f;
						health_col[3] = (float)((c >> 24) & 0xFF) / 255.0f;
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::ColorEdit4("Health Color##health_bar_color", health_col, ImGuiColorEditFlags_NoInputs)) {
							esp::health_bar_color = IM_COL32((int)(health_col[0]*255.0f), (int)(health_col[1]*255.0f), (int)(health_col[2]*255.0f), (int)(health_col[3]*255.0f));
						}
						}
                        // No width/height - simplified fixed sizing based on font scaling

						const char* items[] = { "Top", "Bottom", "Left", "Right" };
						int current = (int)esp::health_bar_position;
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::Combo("Health Position", &current, items, IM_ARRAYSIZE(items))) {
							esp::health_bar_position = (esp::NamePosition)current;
						}

						ImGui::Unindent(20.0f);
					}
				}

				// Armor Bar Toggle and options
				{
					bool ab = esp::armor_bar_enabled;
					if (ImGui::Checkbox("Draw Armor Bar", &ab)) {
						esp::armor_bar_enabled = ab;
					}

					if (esp::armor_bar_enabled) {
						ImGui::Indent(20.0f);
						{
							ImU32 c = esp::armor_bar_color;
						static float armor_col[4] = {0.0f, 0.63f, 1.0f, 1.0f};
						armor_col[0] = (float)((c      ) & 0xFF) / 255.0f;
						armor_col[1] = (float)((c >> 8 ) & 0xFF) / 255.0f;
						armor_col[2] = (float)((c >> 16) & 0xFF) / 255.0f;
						armor_col[3] = (float)((c >> 24) & 0xFF) / 255.0f;
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::ColorEdit4("Armor Color##armor_bar_color", armor_col, ImGuiColorEditFlags_NoInputs)) {
							esp::armor_bar_color = IM_COL32((int)(armor_col[0]*255.0f), (int)(armor_col[1]*255.0f), (int)(armor_col[2]*255.0f), (int)(armor_col[3]*255.0f));
						}
						}
                        // No width/height controls for armor

						const char* items[] = { "Top", "Bottom", "Left", "Right" };
						int current = (int)esp::armor_bar_position;
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::Combo("Armor Position", &current, items, IM_ARRAYSIZE(items))) {
							esp::armor_bar_position = (esp::NamePosition)current;
						}

						ImGui::Unindent(20.0f);
					}
				}

                // Draw Names UI removed

			}

			ImGui::EndTabItem();
		}

		

		// Debug Section (merged Build Information + Performance)
		if (ImGui::BeginTabItem("Debug")) {
			int build_version = FiveM::GetCurrentBuildVersion();
			bool is_supported = FiveM::IsBuildSupported();

			ImGui::Text("Detected Build: b%d", build_version);

			if (is_supported) {
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Verified Offsets");
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Using Placeholder Offsets");
				ImGui::TextWrapped("This build may not work correctly. Check if there is a new update");
			}

			ImGui::Separator();

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
				1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			// ESP performance stats - only show if cache is enabled
			if (esp::get_use_cache()) {
				double hit_ratio = esp::esp_stats.get_hit_ratio();
				ImGui::Text("ESP Cache Hit Ratio: %.1f%%", hit_ratio * 100.0);
				ImGui::Text("ESP Memory Reads: %d", esp::esp_stats.memory_reads);
				ImGui::Text("ESP Batch Reads: %d", esp::esp_stats.batch_reads);  // NEW: Show batch read count
				ImGui::Text("Cache Size: %zu peds", g_pedCacheManager.getCacheSize());

				// NEW: Show batch reading efficiency
				if (esp::get_use_batch_skeleton()) {
					ImGui::Text("Batch Mode: ACTIVE");
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(Optimized)");
				}
				else {
					ImGui::Text("Batch Mode: DISABLED");
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(Standard)");
				}

				if (ImGui::Button("Reset ESP Stats")) {
					esp::esp_stats.reset();
				}

				ImGui::SameLine();
				if (ImGui::Button("Clear Cache")) {
					g_pedCacheManager.clearCache();
					esp::bone_cache.clear();
					esp::clear_skeleton_cache();  // NEW: Also clear skeleton cache
					std::cout << "[ESP] All caches cleared manually" << std::endl;
				}
			}
			else {
				ImGui::Text("ESP Cache: Disabled");
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Enable cache to see performance statistics");
			}

			ImGui::EndTabItem();
		}


		// Exit Section
		if (ImGui::BeginTabItem("Exit Options")) {


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));        // Dark red
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Bright red on hover
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));  // Darker red when pressed

			if (ImGui::Button("Safe Exit", ImVec2(120, 30))) {

				shouldRun = false;
			}

			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Safely exit the application");
		}

		ImGui::EndTabItem();
	}


	ImGui::End();
}

void Overlay::SetForeground(HWND window)
{
	if (!IsWindowInForeground(window))
		BringToForeground(window);
}