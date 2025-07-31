#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <regex>

#include "Config.h" // Include the centralized config file

// Window information structure
struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    std::wstring className;
    std::wstring processName;
    DWORD processId;
    bool isVisible;
    bool isMinimized;
    HICON icon;
    double score = 0.0;
    
    WindowInfo() : hwnd(nullptr), processId(0), isVisible(false), isMinimized(false), icon(nullptr), score(0.0) {}
};

// Utility functions
namespace Utils {
    std::wstring GetProcessName(DWORD processId);
    HICON GetWindowIcon(HWND hwnd);
    void CenterWindow(HWND hwnd, int width, int height);
    bool IsValidWindow(HWND hwnd);
    UINT StringToVK(const std::string& key);
    UINT LoadHotkeySetting();
} 