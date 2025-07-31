#include "Utils.h"
#include "Config.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <tlhelp32.h>
#include <shellapi.h>
#include <map>
#include <vector>

namespace Utils {

std::wstring GetProcessName(DWORD processId) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return L"";
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == processId) {
                CloseHandle(hSnapshot);
                return std::wstring(pe32.szExeFile);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return L"";
}

HICON GetWindowIcon(HWND hwnd) {
    // Try to get the icon from the window
    HICON icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0));
    if (!icon) {
        icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0));
    }
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM));
    }
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
    }

    // If still no icon, try to get it from the executable
    if (!icon) {
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            wchar_t exePath[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
                SHFILEINFOW fileInfo;
                if (SHGetFileInfoW(exePath, 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_SMALLICON)) {
                    icon = fileInfo.hIcon;
                }
            }
            CloseHandle(hProcess);
        }
    }

    return icon;
}

void CenterWindow(HWND hwnd, int width, int height) {
    POINT p;
    GetCursorPos(&p);

    HMONITOR hMonitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);

    int screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

    int x = mi.rcMonitor.left + (screenWidth - width) / 2;
    int y = mi.rcMonitor.top + (screenHeight - height) / 2;

    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
}

bool IsValidWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    // Basic properties check
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }
    if (GetParent(hwnd) != nullptr) {
        return false;
    }

    // Check for a valid title
    wchar_t windowTitle[512];
    if (GetWindowTextW(hwnd, windowTitle, 512) == 0) {
        return false;
    }
    std::wstring titleStr(windowTitle);

    // Get process info
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    std::wstring processName = GetProcessName(processId);

    // --- Custom Filtering Logic ---
    if (!processName.empty()) {
        std::transform(processName.begin(), processName.end(), processName.begin(), ::towlower);
        for (const auto& excludedProc : Config::EXCLUDED_PROCESSES) {
            if (processName == excludedProc) {
                return false;
            }
        }
    }

    if (!titleStr.empty()) {
        std::wstring lowerTitle = titleStr;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
        for (const auto& excludedTitle : Config::EXCLUDED_TITLES) {
            if (lowerTitle.find(excludedTitle) != std::wstring::npos) {
                return false;
            }
        }
    }

    // --- System-level Filtering ---
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring classNameStr(className);
    if (classNameStr == L"Shell_TrayWnd" ||
        classNameStr == L"Progman" ||
        classNameStr == L"Windows.UI.Core.CoreWindow") { // UWP app host
        return false;
    }

    return true;
}

UINT StringToVK(const std::string& key) {
    static const std::map<std::string, UINT> keyMap = {
        {"LBUTTON", VK_LBUTTON},
        {"RBUTTON", VK_RBUTTON},
        {"CANCEL", VK_CANCEL},
        {"MBUTTON", VK_MBUTTON},
        {"XBUTTON1", VK_XBUTTON1},
        {"XBUTTON2", VK_XBUTTON2},
        {"BACK", VK_BACK},
        {"TAB", VK_TAB},
        {"CLEAR", VK_CLEAR},
        {"RETURN", VK_RETURN},
        {"SHIFT", VK_SHIFT},
        {"CONTROL", VK_CONTROL},
        {"ALT", VK_MENU},
        {"PAUSE", VK_PAUSE},
        {"CAPITAL", VK_CAPITAL},
        {"ESCAPE", VK_ESCAPE},
        {"SPACE", VK_SPACE},
        {"PRIOR", VK_PRIOR},
        {"NEXT", VK_NEXT},
        {"END", VK_END},
        {"HOME", VK_HOME},
        {"LEFT", VK_LEFT},
        {"UP", VK_UP},
        {"RIGHT", VK_RIGHT},
        {"DOWN", VK_DOWN},
        {"SELECT", VK_SELECT},
        {"PRINT", VK_PRINT},
        {"EXECUTE", VK_EXECUTE},
        {"SNAPSHOT", VK_SNAPSHOT},
        {"INSERT", VK_INSERT},
        {"DELETE", VK_DELETE},
        {"HELP", VK_HELP},
        {"LWIN", VK_LWIN},
        {"RWIN", VK_RWIN},
        {"APPS", VK_APPS},
        {"SLEEP", VK_SLEEP},
        {"NUMPAD0", VK_NUMPAD0},
        {"NUMPAD1", VK_NUMPAD1},
        {"NUMPAD2", VK_NUMPAD2},
        {"NUMPAD3", VK_NUMPAD3},
        {"NUMPAD4", VK_NUMPAD4},
        {"NUMPAD5", VK_NUMPAD5},
        {"NUMPAD6", VK_NUMPAD6},
        {"NUMPAD7", VK_NUMPAD7},
        {"NUMPAD8", VK_NUMPAD8},
        {"NUMPAD9", VK_NUMPAD9},
        {"MULTIPLY", VK_MULTIPLY},
        {"ADD", VK_ADD},
        {"SEPARATOR", VK_SEPARATOR},
        {"SUBTRACT", VK_SUBTRACT},
        {"DECIMAL", VK_DECIMAL},
        {"DIVIDE", VK_DIVIDE},
        {"F1", VK_F1},
        {"F2", VK_F2},
        {"F3", VK_F3},
        {"F4", VK_F4},
        {"F5", VK_F5},
        {"F6", VK_F6},
        {"F7", VK_F7},
        {"F8", VK_F8},
        {"F9", VK_F9},
        {"F10", VK_F10},
        {"F11", VK_F11},
        {"F12", VK_F12},
        {"NUMLOCK", VK_NUMLOCK},
        {"SCROLL", VK_SCROLL},
        {"LSHIFT", VK_LSHIFT},
        {"RSHIFT", VK_RSHIFT},
        {"LCONTROL", VK_LCONTROL},
        {"RCONTROL", VK_RCONTROL},
        {"LMENU", VK_LMENU},
        {"RMENU", VK_RMENU}
    };

    std::string upperKey = key;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    auto it = keyMap.find(upperKey);
    if (it != keyMap.end()) {
        return it->second;
    }

    // For alphanumeric keys
    if (upperKey.length() == 1) {
        char c = upperKey[0];
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            return VkKeyScanA(c);
        }
    }

    return 0; // Return 0 if not found
}

UINT LoadHotkeySetting() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string::size_type pos = std::string(exePath).find_last_of("\\/");
    std::string configPath = std::string(exePath).substr(0, pos) + "\\config.ini";

    char hotkeyStr[50];
    GetPrivateProfileStringA("Hotkeys", "Activation", "RSHIFT", hotkeyStr, 50, configPath.c_str());

    UINT vkCode = StringToVK(hotkeyStr);
    if (vkCode == 0) {
        return VK_RSHIFT; // Default if key is invalid
    }

    return vkCode;
}

} // namespace Utils 