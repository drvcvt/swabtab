#include "WindowManager.h"
#include "Utils.h"
#include <algorithm>

WindowManager::WindowManager() {
}

WindowManager::~WindowManager() {
}

std::vector<WindowInfo> WindowManager::GetAllWindows() {
    m_windows.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
    return m_windows;
}

bool WindowManager::ActivateWindow(HWND hwnd) {
    if (!IsWindowValid(hwnd)) {
        return false;
    }

    bool wasIconic = IsIconic(hwnd);

    // Simulate a key press to allow SetForegroundWindow to work
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    // If window is minimized, restore it first
    if (wasIconic) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    
    // To handle cases where SetForegroundWindow might fail, 
    // we can attach our thread's input to the target window's thread.
    DWORD currentThreadId = GetCurrentThreadId();
    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, nullptr);

    if (currentThreadId != targetThreadId) {
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    } else {
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    
    // This is a common trick to force a window to the top of the Z-order,
    // ensuring it appears correctly in the Alt-Tab list.
    // We briefly make it the topmost window, then remove that status,
    // which pushes it to the top of the non-topmost stack.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // After activating, always center the cursor in the window
    // to ensure compatibility with mouse-driven tilers.
    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        SetCursorPos(rc.left + (rc.right - rc.left) / 2, rc.top + (rc.bottom - rc.top) / 2);
    }

    return true;
}

bool WindowManager::IsWindowValid(HWND hwnd) {
    return Utils::IsValidWindow(hwnd);
}

void WindowManager::RefreshWindows() {
    GetAllWindows();
}

BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowManager* manager = reinterpret_cast<WindowManager*>(lParam);
    
    if (manager->ShouldIncludeWindow(hwnd)) {
        WindowInfo info = manager->CreateWindowInfo(hwnd);
        manager->m_windows.push_back(info);
    }
    
    return TRUE; // Continue enumeration
}

WindowInfo WindowManager::CreateWindowInfo(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    info.title = GetWindowTitle(hwnd);
    info.className = GetWindowClassName(hwnd);
    
    // Get process ID and name
    GetWindowThreadProcessId(hwnd, &info.processId);
    info.processName = Utils::GetProcessName(info.processId);
    
    // Append process name to title for better searchability
    if (!info.processName.empty()) {
        info.title += L" (" + info.processName + L")";
    }

    // Window state
    info.isVisible = IsWindowVisible(hwnd) != FALSE;
    info.isMinimized = IsIconic(hwnd) != FALSE;
    
    // Get icon
    info.icon = Utils::GetWindowIcon(hwnd);
    
    return info;
}

bool WindowManager::ShouldIncludeWindow(HWND hwnd) {
    return Utils::IsValidWindow(hwnd);
}

std::wstring WindowManager::GetWindowTitle(HWND hwnd) {
    wchar_t title[512];
    int length = GetWindowTextW(hwnd, title, 512);
    if (length > 0) {
        return std::wstring(title, length);
    }
    return L"";
}

std::wstring WindowManager::GetWindowClassName(HWND hwnd) {
    wchar_t className[256];
    int length = GetClassNameW(hwnd, className, 256);
    if (length > 0) {
        return std::wstring(className, length);
    }
    return L"";
} 