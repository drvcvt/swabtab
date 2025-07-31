#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Custom message for keyboard events from the hook
constexpr UINT WM_APP_KEYBOARD_EVENT = WM_APP + 1;

namespace Config {
    // Window settings
    extern int WINDOW_WIDTH;
    extern int WINDOW_HEIGHT;

    // UI layout
    extern int ITEM_HEIGHT;
    extern int PADDING;
    extern int ICON_SIZE;

    // Fonts
    extern std::wstring FONT_NAME;
    extern int FONT_SIZE;

    // Colors
    extern COLORREF BG_COLOR;
    extern COLORREF TEXT_COLOR;
    extern COLORREF SELECTED_COLOR;
    extern COLORREF HIGHLIGHT_COLOR;
    extern COLORREF BORDER_COLOR;

    // Window Filters
    extern std::vector<std::wstring> EXCLUDED_PROCESSES;
    extern std::vector<std::wstring> EXCLUDED_TITLES;

    void LoadConfig(); // Function to load all settings
}