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

HICON GetWindowIcon(HWND hwnd, bool& destroyIcon) {
    destroyIcon = false;

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
                    destroyIcon = true;
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
            std::wstring lowerExcludedProc = excludedProc;
            std::transform(lowerExcludedProc.begin(), lowerExcludedProc.end(), lowerExcludedProc.begin(), ::towlower);
            if (processName == lowerExcludedProc) {
                return false;
            }
        }
    }

    if (!titleStr.empty()) {
        std::wstring lowerTitle = titleStr;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
        for (const auto& excludedTitle : Config::EXCLUDED_TITLES) {
            std::wstring lowerExcludedTitle = excludedTitle;
            std::transform(lowerExcludedTitle.begin(), lowerExcludedTitle.end(), lowerExcludedTitle.begin(), ::towlower);
            if (lowerTitle.find(lowerExcludedTitle) != std::wstring::npos) {
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

UINT StringToVK(const std::wstring& key) {
    static const std::map<std::wstring, UINT> keyMap = {
        {L"LBUTTON", VK_LBUTTON},
        {L"RBUTTON", VK_RBUTTON},
        {L"CANCEL", VK_CANCEL},
        {L"MBUTTON", VK_MBUTTON},
        {L"XBUTTON1", VK_XBUTTON1},
        {L"XBUTTON2", VK_XBUTTON2},
        {L"BACK", VK_BACK},
        {L"TAB", VK_TAB},
        {L"CLEAR", VK_CLEAR},
        {L"RETURN", VK_RETURN},
        {L"SHIFT", VK_SHIFT},
        {L"CONTROL", VK_CONTROL},
        {L"ALT", VK_MENU},
        {L"PAUSE", VK_PAUSE},
        {L"CAPITAL", VK_CAPITAL},
        {L"ESCAPE", VK_ESCAPE},
        {L"SPACE", VK_SPACE},
        {L"PRIOR", VK_PRIOR},
        {L"NEXT", VK_NEXT},
        {L"END", VK_END},
        {L"HOME", VK_HOME},
        {L"LEFT", VK_LEFT},
        {L"UP", VK_UP},
        {L"RIGHT", VK_RIGHT},
        {L"DOWN", VK_DOWN},
        {L"SELECT", VK_SELECT},
        {L"PRINT", VK_PRINT},
        {L"EXECUTE", VK_EXECUTE},
        {L"SNAPSHOT", VK_SNAPSHOT},
        {L"INSERT", VK_INSERT},
        {L"DELETE", VK_DELETE},
        {L"HELP", VK_HELP},
        {L"LWIN", VK_LWIN},
        {L"RWIN", VK_RWIN},
        {L"APPS", VK_APPS},
        {L"SLEEP", VK_SLEEP},
        {L"NUMPAD0", VK_NUMPAD0},
        {L"NUMPAD1", VK_NUMPAD1},
        {L"NUMPAD2", VK_NUMPAD2},
        {L"NUMPAD3", VK_NUMPAD3},
        {L"NUMPAD4", VK_NUMPAD4},
        {L"NUMPAD5", VK_NUMPAD5},
        {L"NUMPAD6", VK_NUMPAD6},
        {L"NUMPAD7", VK_NUMPAD7},
        {L"NUMPAD8", VK_NUMPAD8},
        {L"NUMPAD9", VK_NUMPAD9},
        {L"MULTIPLY", VK_MULTIPLY},
        {L"ADD", VK_ADD},
        {L"SEPARATOR", VK_SEPARATOR},
        {L"SUBTRACT", VK_SUBTRACT},
        {L"DECIMAL", VK_DECIMAL},
        {L"DIVIDE", VK_DIVIDE},
        {L"F1", VK_F1},
        {L"F2", VK_F2},
        {L"F3", VK_F3},
        {L"F4", VK_F4},
        {L"F5", VK_F5},
        {L"F6", VK_F6},
        {L"F7", VK_F7},
        {L"F8", VK_F8},
        {L"F9", VK_F9},
        {L"F10", VK_F10},
        {L"F11", VK_F11},
        {L"F12", VK_F12},
        {L"NUMLOCK", VK_NUMLOCK},
        {L"SCROLL", VK_SCROLL},
        {L"LSHIFT", VK_LSHIFT},
        {L"RSHIFT", VK_RSHIFT},
        {L"LCONTROL", VK_LCONTROL},
        {L"RCONTROL", VK_RCONTROL},
        {L"LMENU", VK_LMENU},
        {L"RMENU", VK_RMENU}
    };

    std::wstring upperKey = key;
    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });

    auto it = keyMap.find(upperKey);
    if (it != keyMap.end()) {
        return it->second;
    }

    // For alphanumeric keys
    if (upperKey.length() == 1) {
        wchar_t c = upperKey[0];
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
            return static_cast<UINT>(VkKeyScanW(c));
        }
    }

    return 0; // Return 0 if not found
}

UINT LoadHotkeySetting() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring::size_type pos = std::wstring(exePath).find_last_of(L"\\/");
    std::wstring configPath = std::wstring(exePath).substr(0, pos) + L"\\config.ini";

    wchar_t hotkeyStr[50];
    GetPrivateProfileStringW(L"Hotkeys", L"Activation", L"RSHIFT", hotkeyStr, 50, configPath.c_str());

    UINT vkCode = StringToVK(hotkeyStr);
    if (vkCode == 0) {
        return VK_RSHIFT; // Default if key is invalid
    }

    return vkCode;
}

} // namespace Utils 