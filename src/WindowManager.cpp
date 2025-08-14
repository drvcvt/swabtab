#include "WindowManager.h"
#include "Utils.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

struct IconCacheEntry {
    HICON icon;
    bool destroy;
};

std::unordered_map<DWORD, std::wstring> g_processNameCache;
std::unordered_map<std::wstring, IconCacheEntry> g_iconCache;

WindowManager::WindowManager() {
}

WindowManager::~WindowManager() {
    for (auto& entry : g_iconCache) {
        if (entry.second.destroy && entry.second.icon) {
            DestroyIcon(entry.second.icon);
        }
    }
    g_iconCache.clear();
}

std::vector<WindowInfo> WindowManager::GetAllWindows() {
    m_windows.clear();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));

    std::unordered_set<std::wstring> activeProcesses;
    for (const auto& info : m_windows) {
        activeProcesses.insert(info.processName);
    }

    for (auto it = g_iconCache.begin(); it != g_iconCache.end(); ) {
        if (activeProcesses.find(it->first) == activeProcesses.end()) {
            if (it->second.destroy && it->second.icon) {
                DestroyIcon(it->second.icon);
            }
            it = g_iconCache.erase(it);
        } else {
            ++it;
        }
    }
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
    auto procIt = g_processNameCache.find(info.processId);
    if (procIt != g_processNameCache.end()) {
        info.processName = procIt->second;
    } else {
        info.processName = Utils::GetProcessName(info.processId);
        g_processNameCache[info.processId] = info.processName;
    }
    
    // Append process name to title for better searchability
    if (!info.processName.empty()) {
        info.title += L" (" + info.processName + L")";
    }

    // Precompute lowercase variants for faster searching
    info.titleLower = info.title;
    std::transform(info.titleLower.begin(), info.titleLower.end(), info.titleLower.begin(), ::towlower);

    info.processLower = Utils::RemoveFileExtension(info.processName);
    std::transform(info.processLower.begin(), info.processLower.end(), info.processLower.begin(), ::towlower);

    // Window state
    info.isVisible = IsWindowVisible(hwnd) != FALSE;
    info.isMinimized = IsIconic(hwnd) != FALSE;

    // Get icon
    auto iconIt = g_iconCache.find(info.processName);
    if (iconIt != g_iconCache.end()) {
        info.icon = iconIt->second.icon;
        info.destroyIcon = false;
    } else {
        IconCacheEntry entry{};
        entry.icon = Utils::GetWindowIcon(hwnd, entry.destroy);
        g_iconCache[info.processName] = entry;
        info.icon = entry.icon;
        info.destroyIcon = false;
    }
    
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