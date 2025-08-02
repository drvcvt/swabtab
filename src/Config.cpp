#include "Config.h"
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cwctype>

// Helper function to trim from both ends
static inline std::wstring trim(const std::wstring& s) {
    const std::wstring WHITESPACE = L" \n\r\t\f\v";
    size_t first = s.find_first_not_of(WHITESPACE);
    if (std::wstring::npos == first) {
        return s;
    }
    size_t last = s.find_last_not_of(WHITESPACE);
    return s.substr(first, (last - first + 1));
}

// Helper to split string by delimiter
static std::vector<std::wstring> split(const std::wstring& s, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstring token;
    std::wistringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

// Helper to parse color from "R,G,B" string
static COLORREF parseColor(const std::wstring& colorStr, COLORREF defaultColor) {
    std::wstringstream ss(colorStr);
    int r, g, b;
    wchar_t comma;
    if (ss >> r >> comma >> g >> comma >> b) {
        return RGB(r, g, b);
    }
    return defaultColor;
}

namespace Config {
    // Default values
    int WINDOW_WIDTH = 680;
    int WINDOW_HEIGHT = 450;
    int ITEM_HEIGHT = 40;
    int PADDING = 15;
    int ICON_SIZE = 24;
    std::wstring FONT_NAME = L"Segoe UI";
    int FONT_SIZE = 16;
    COLORREF BG_COLOR = RGB(32, 32, 32);
    COLORREF TEXT_COLOR = RGB(240, 240, 240);
    COLORREF SELECTED_COLOR = RGB(55, 55, 55);
    COLORREF HIGHLIGHT_COLOR = RGB(0, 120, 215);
    COLORREF BORDER_COLOR = RGB(80, 80, 80);
    std::vector<std::wstring> EXCLUDED_PROCESSES;
    std::vector<std::wstring> EXCLUDED_TITLES;

    void LoadConfig() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring::size_type pos = std::wstring(exePath).find_last_of(L"\\/");
        std::wstring configPath = std::wstring(exePath).substr(0, pos) + L"\\config.ini";

        // Appearance settings
        WINDOW_WIDTH = GetPrivateProfileIntW(L"Appearance", L"WindowWidth", 680, configPath.c_str());
        WINDOW_HEIGHT = GetPrivateProfileIntW(L"Appearance", L"WindowHeight", 450, configPath.c_str());
        ITEM_HEIGHT = GetPrivateProfileIntW(L"Appearance", L"ItemHeight", 40, configPath.c_str());
        PADDING = GetPrivateProfileIntW(L"Appearance", L"Padding", 15, configPath.c_str());
        ICON_SIZE = GetPrivateProfileIntW(L"Appearance", L"IconSize", 24, configPath.c_str());

        wchar_t fontNameStr[100];
        GetPrivateProfileStringW(L"Appearance", L"FontName", L"Segoe UI", fontNameStr, 100, configPath.c_str());
        FONT_NAME = fontNameStr;

        FONT_SIZE = GetPrivateProfileIntW(L"Appearance", L"FontSize", 16, configPath.c_str());

        wchar_t colorStr[50];
        GetPrivateProfileStringW(L"Appearance", L"BackgroundColor", L"32,32,32", colorStr, 50, configPath.c_str());
        BG_COLOR = parseColor(colorStr, RGB(32, 32, 32));
        GetPrivateProfileStringW(L"Appearance", L"TextColor", L"240,240,240", colorStr, 50, configPath.c_str());
        TEXT_COLOR = parseColor(colorStr, RGB(240, 240, 240));
        GetPrivateProfileStringW(L"Appearance", L"SelectedColor", L"55,55,55", colorStr, 50, configPath.c_str());
        SELECTED_COLOR = parseColor(colorStr, RGB(55, 55, 55));
        GetPrivateProfileStringW(L"Appearance", L"HighlightColor", L"0,120,215", colorStr, 50, configPath.c_str());
        HIGHLIGHT_COLOR = parseColor(colorStr, RGB(0, 120, 215));
        GetPrivateProfileStringW(L"Appearance", L"BorderColor", L"80,80,80", colorStr, 50, configPath.c_str());
        BORDER_COLOR = parseColor(colorStr, RGB(80, 80, 80));

        // Window Filters
        wchar_t buffer[2048];
        GetPrivateProfileStringW(L"WindowFilters", L"ExcludeProcessNames", L"", buffer, 2048, configPath.c_str());
        EXCLUDED_PROCESSES = split(buffer, L',');
        for (auto& proc : EXCLUDED_PROCESSES) {
            std::transform(proc.begin(), proc.end(), proc.begin(), ::towlower);
        }

        GetPrivateProfileStringW(L"WindowFilters", L"ExcludeTitles", L"", buffer, 2048, configPath.c_str());
        EXCLUDED_TITLES = split(buffer, L',');
        for (auto& title : EXCLUDED_TITLES) {
            std::transform(title.begin(), title.end(), title.begin(), ::towlower);
        }
    }
}
