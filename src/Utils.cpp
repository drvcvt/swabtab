#include "Utils.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <tlhelp32.h>
#include <shellapi.h>

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
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
}

bool IsValidWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    // Check if window has a title
    wchar_t title[256];
    if (GetWindowTextLengthW(hwnd) == 0) {
        return false;
    }

    // Exclude the owner window of the switcher itself if it's ever visible
    // This check needs to be more robust, potentially passing the switcher's HWND
    // GetClassNameW(hwnd, title, 256);
    // if (wcscmp(title, L"TabSwitcherWindow") == 0) return false;


    // Check for tool windows
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }

    // Check if it's a child window
     if (GetParent(hwnd) != nullptr) {
        return false;
    }

    // Check window styles
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (!(style & WS_CAPTION)) {
       // return false; // Usually, windows without captions are not interesting
    }


    // Further filtering based on class name or other properties can be added here
    // For example, filtering out system windows or specific applications.
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

} // namespace Utils 