#pragma once
#include <windows.h>
#include <string>

// Custom message for keyboard events from the hook
constexpr UINT WM_APP_KEYBOARD_EVENT = WM_APP + 1;

namespace Config {
    // Window settings
    constexpr int WINDOW_WIDTH = 680; // Increased width
    constexpr int WINDOW_HEIGHT = 450; // Increased height

    // UI layout
    constexpr int ITEM_HEIGHT = 40; // Increased item height for more space
    constexpr int PADDING = 15;     // Increased padding
    constexpr int ICON_SIZE = 24;

    // Colors - New modern, dark theme
    constexpr COLORREF BG_COLOR = RGB(32, 32, 32);           // Dark gray background
    constexpr COLORREF TEXT_COLOR = RGB(240, 240, 240);       // Light gray text
    constexpr COLORREF SELECTED_COLOR = RGB(55, 55, 55);      // Slightly lighter gray for selection
    constexpr COLORREF HIGHLIGHT_COLOR = RGB(0, 120, 215);    // System highlight blue
    constexpr COLORREF BORDER_COLOR = RGB(80, 80, 80);        // Border color for separation
} 