#pragma once
#include <windows.h>

constexpr UINT WM_SWABTAB_WINDOW_SELECTED = WM_USER + 100;
constexpr UINT WM_WINTILE_SNAP_REQUEST = WM_USER + 101;

constexpr const wchar_t* WINTILE_WINDOW_CLASS = L"WinVimTilerWindowClass";
constexpr const wchar_t* SWABTAB_WINDOW_CLASS = L"TabSwitcherWindowClass";

struct WindowSelectionData {
    HWND selectedWindow;
    DWORD processId;
    wchar_t title[256];
    wchar_t className[256];
};
