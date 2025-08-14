#pragma once

#include "Utils.h"
#include <vector>
#include <functional>

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // Get all windows
    std::vector<WindowInfo> GetAllWindows();
    
    // Window operations
    bool ActivateWindow(HWND hwnd);
    bool IsWindowValid(HWND hwnd);

    // Refresh window list
    void RefreshWindows();

    // Expose helper methods for single-window updates
    WindowInfo CreateWindowInfo(HWND hwnd);
    bool ShouldIncludeWindow(HWND hwnd);

private:
    std::vector<WindowInfo> m_windows;
    
    // Static callback for EnumWindows
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    
    // Helper methods
    std::wstring GetWindowTitle(HWND hwnd);
    std::wstring GetWindowClassName(HWND hwnd);
};