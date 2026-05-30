
#include "kmbox.h"
#include <iostream>

namespace kmbox
{
	HANDLE serial_handle = nullptr;

	bool connected = false;

	int clamp(int i)
	{
		if (i > 127)
			i = 127;
		if (i < -128)
			i = -128;

		return i;
	}

	std::string find_port(const std::string& targetDescription)
	{
		HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);
		if (hDevInfo == INVALID_HANDLE_VALUE) return "";

		SP_DEVINFO_DATA deviceInfoData;
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); ++i)
		{
			char buf[512];
			DWORD nSize = 0;

			if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &deviceInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)buf, sizeof(buf), &nSize) && nSize > 0)
			{
				buf[nSize] = '\0';
				std::string deviceDescription = buf;

				size_t comPos = deviceDescription.find("COM");
				size_t endPos = deviceDescription.find(")", comPos);

				if (comPos != std::string::npos && endPos != std::string::npos && deviceDescription.find(targetDescription) != std::string::npos)
				{
					SetupDiDestroyDeviceInfoList(hDevInfo);
					return deviceDescription.substr(comPos, endPos - comPos);
				}
			}
		}
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return "";
	}

	void KmboxInitialize(std::string port)
	{

		connected = false;
		if (serial_handle)
		{
			CloseHandle(serial_handle);
			serial_handle = NULL;
		}
		//	wprintf(L"Connecting to KMBOX on port %ls\n",port.c_str());
		//std::string str = std::string(port.begin(), port.end());
		port = "\\\\.\\" + find_port("USB-SERIAL CH340");
		printf("Connecting to KMBOX on port %s\n", port.c_str());

		serial_handle = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

		if (serial_handle == INVALID_HANDLE_VALUE)
		{
			// print the serialhandle
			printf("Serial handle: %d\n", serial_handle);
			printf("Failed to open serial port!\n");
			return;

		}

		if (!SetupComm(serial_handle, 8192, 8192))
		{
			printf("Failed to setup serial port!\n");
			CloseHandle(serial_handle);
			return;
		}

		COMMTIMEOUTS timeouts = { 0 };
		if (!GetCommTimeouts(serial_handle, &timeouts))
		{
			CloseHandle(serial_handle);
			return;
		}
		timeouts.ReadIntervalTimeout = 0xFFFFFFFF;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 2000;

		if (!SetCommTimeouts(serial_handle, &timeouts))
		{

			CloseHandle(serial_handle);
			return;
		}

		PurgeComm(serial_handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

		DCB dcbSerialParams = { 0 };
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

		if (!GetCommState(serial_handle, &dcbSerialParams))
		{
			printf("Failed to get serial state!\n");
			CloseHandle(serial_handle);
			return;
		}

		int baud = 115200;
		dcbSerialParams.BaudRate = baud;
		dcbSerialParams.ByteSize = 8;
		dcbSerialParams.StopBits = ONESTOPBIT;
		dcbSerialParams.Parity = NOPARITY;

		if (!SetCommState(serial_handle, &dcbSerialParams))
		{
			printf("Failed to set serial parameters!\n");
			CloseHandle(serial_handle);
			return;
		}

		//printf("Connected to KMBOX on port %s\n", std::string(port).c_str());
		connected = true;
	}

	void SendCommand(const std::string& command)
	{
		DWORD bytesWritten;
		if (!WriteFile(serial_handle, command.c_str(), command.length(), &bytesWritten, NULL))
		{
			//	printf("Failed to write to serial port!\n");
		}
	}

	void move(int x, int y)
	{
		if (!connected)
		{
			//	printf("not connected?\n");
			return;
		}
		x = clamp(x);
		y = clamp(y);
		std::string command = "km.move(" + std::to_string(x) + "," + std::to_string(y) + ")\r\n";
		SendCommand(command.c_str());
	}

	void left_click()
	{
		if (!connected)
		{
			//	printf("not connected?\n");
			return;
		}
		std::string command = "km.left(1)\r\n";
		SendCommand(command.c_str());
	}

	void left_click_release()
	{
		if (!connected)
		{
			//	printf("not connected?\n");
			return;
		}
		std::string command = "km.left(0)\r\n";
		SendCommand(command.c_str());
	}

	// FIXED: Mouse button detection function
	bool IsDown(int virtual_key)
	{
		if (!connected)
			return false;

		// REMOVED THE EARLY RETURN FALSE - Now the function actually works!
		std::string command;

		// Map virtual keys to KmBox commands
		switch (virtual_key) {
		case 1: // VK_LBUTTON - Left mouse button
			command = "km.left()\r\n";
			break;
		case 2: // VK_RBUTTON - Right mouse button  
			command = "km.right()\r\n";
			break;
		case 4: // VK_MBUTTON - Middle mouse button
			command = "km.middle()\r\n";
			break;
		case 5: // VK_XBUTTON1 - Mouse 4
			command = "km.ms1()\r\n";
			break;
		case 6: // VK_XBUTTON2 - Mouse 5
			command = "km.ms2()\r\n";
			break;
		default:
			return false; // Unsupported key
		}

		SendCommand(command);

		// Read response from KmBox
		char readBuffer[256];
		DWORD bytesRead;
		if (!ReadFile(serial_handle, readBuffer, sizeof(readBuffer) - 1, &bytesRead, NULL))
		{
			std::cerr << "Failed to read from serial port!" << std::endl;
			return false;
		}

		if (bytesRead > 0)
		{
			readBuffer[bytesRead] = '\0';

			// Check if response indicates button is pressed (KmBox returns "1" for pressed)
			if (strstr(readBuffer, "1") != nullptr)
				return true;
		}
		return false;
	}

	// NEW: Enhanced key detection functions for aimbot
	bool IsKeyJustPressed(int virtual_key)
	{
		static std::map<int, bool> previous_states;

		bool current_state = IsDown(virtual_key);
		bool previous_state = previous_states[virtual_key];

		previous_states[virtual_key] = current_state;

		// Return true only if key is currently pressed and wasn't pressed before
		return current_state && !previous_state;
	}

	bool IsKeyJustReleased(int virtual_key)
	{
		static std::map<int, bool> previous_states;

		bool current_state = IsDown(virtual_key);
		bool previous_state = previous_states[virtual_key];

		previous_states[virtual_key] = current_state;

		// Return true only if key is currently not pressed but was pressed before
		return !current_state && previous_state;
	}

	// NEW: Helper function to get available mouse buttons
	std::vector<std::string> GetAvailableMouseButtons()
	{
		return {
			"Left Mouse",
			"Right Mouse",
			"Middle Mouse",
			"Mouse 4",
			"Mouse 5"
		};
	}

	// NEW: Helper function to convert button name to virtual key code
	int GetMouseButtonKeyCode(const std::string& button_name)
	{
		if (button_name == "Left Mouse") return 1;
		if (button_name == "Right Mouse") return 2;
		if (button_name == "Middle Mouse") return 4;
		if (button_name == "Mouse 4") return 5;
		if (button_name == "Mouse 5") return 6;
		return 2; // Default to right mouse
	}
}