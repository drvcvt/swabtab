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
static COLORREF parseColor(const std::string& colorStr, COLORREF defaultColor) {
    std::stringstream ss(colorStr);
    int r, g, b;
    char comma;
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
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string::size_type pos = std::string(exePath).find_last_of("\\/");
        std::string configPath = std::string(exePath).substr(0, pos) + "\\config.ini";

        // Appearance settings
        WINDOW_WIDTH = GetPrivateProfileIntA("Appearance", "WindowWidth", 680, configPath.c_str());
        WINDOW_HEIGHT = GetPrivateProfileIntA("Appearance", "WindowHeight", 450, configPath.c_str());
        ITEM_HEIGHT = GetPrivateProfileIntA("Appearance", "ItemHeight", 40, configPath.c_str());
        PADDING = GetPrivateProfileIntA("Appearance", "Padding", 15, configPath.c_str());
        ICON_SIZE = GetPrivateProfileIntA("Appearance", "IconSize", 24, configPath.c_str());

        char fontNameStr[100];
        GetPrivateProfileStringA("Appearance", "FontName", "Segoe UI", fontNameStr, 100, configPath.c_str());
        int requiredSize = MultiByteToWideChar(CP_UTF8, 0, fontNameStr, -1, NULL, 0);
        FONT_NAME.resize(requiredSize - 1);
        MultiByteToWideChar(CP_UTF8, 0, fontNameStr, -1, &FONT_NAME[0], requiredSize);

        FONT_SIZE = GetPrivateProfileIntA("Appearance", "FontSize", 16, configPath.c_str());

        char colorStr[50];
        GetPrivateProfileStringA("Appearance", "BackgroundColor", "32,32,32", colorStr, 50, configPath.c_str());
        BG_COLOR = parseColor(colorStr, RGB(32, 32, 32));
        GetPrivateProfileStringA("Appearance", "TextColor", "240,240,240", colorStr, 50, configPath.c_str());
        TEXT_COLOR = parseColor(colorStr, RGB(240, 240, 240));
        GetPrivateProfileStringA("Appearance", "SelectedColor", "55,55,55", colorStr, 50, configPath.c_str());
        SELECTED_COLOR = parseColor(colorStr, RGB(55, 55, 55));
        GetPrivateProfileStringA("Appearance", "HighlightColor", "0,120,215", colorStr, 50, configPath.c_str());
        HIGHLIGHT_COLOR = parseColor(colorStr, RGB(0, 120, 215));
        GetPrivateProfileStringA("Appearance", "BorderColor", "80,80,80", colorStr, 50, configPath.c_str());
        BORDER_COLOR = parseColor(colorStr, RGB(80, 80, 80));

        // Window Filters
        wchar_t buffer[2048];
        GetPrivateProfileStringW(L"WindowFilters", L"ExcludeProcessNames", L"", buffer, 2048, std::wstring(configPath.begin(), configPath.end()).c_str());
        EXCLUDED_PROCESSES = split(buffer, L',');
        for (auto& proc : EXCLUDED_PROCESSES) {
            std::transform(proc.begin(), proc.end(), proc.begin(), ::towlower);
        }

        GetPrivateProfileStringW(L"WindowFilters", L"ExcludeTitles", L"", buffer, 2048, std::wstring(configPath.begin(), configPath.end()).c_str());
        EXCLUDED_TITLES = split(buffer, L',');
        for (auto& title : EXCLUDED_TITLES) {
            std::transform(title.begin(), title.end(), title.begin(), ::towlower);
        }
    }
}
