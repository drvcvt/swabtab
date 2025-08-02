#pragma once

#include "Utils.h"
#include "WindowManager.h"
#include "Config.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <dwmapi.h>

constexpr UINT WM_APP_KEYDOWN = WM_APP + 1;

#include <thread>
#include <mutex>
#include <atomic>

class TabSwitcher {
public:
    TabSwitcher();
    ~TabSwitcher();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const { return m_isVisible.load(); }
    HWND GetHwnd() const { return m_hwnd; }

private:
    void RegisterWindowClass();
    void UnregisterWindowClass();
    void CenterOnScreen();
    void RegisterThumbnail(HWND targetHwnd);
    void UnregisterThumbnail();
    void FilterWindows();
    void EnsureSelectionIsVisible();
    
    // Background thread for updating window list
    void StartWindowUpdater();
    void StopWindowUpdater();
    void UpdateWindowsInBackground();

    // Fuzzy matching scoring methods
    double CalculateFuzzyScore(const std::wstring& search, const std::wstring& target);
    double CalculatePositionScore(const std::wstring& search, const std::wstring& target);
    double CalculatePrefixScore(const std::wstring& search, const std::wstring& target);
    double CalculateSequentialScore(const std::wstring& search, const std::wstring& target);

    // Window management
    HWND m_hwnd;
    HTHUMBNAIL m_hThumbnail;
    HINSTANCE m_hInstance;
    std::atomic<bool> m_isVisible{false};
    
    // Window data
    std::unique_ptr<WindowManager> m_windowManager;
    std::vector<WindowInfo> m_windows;
    std::vector<WindowInfo> m_filteredWindows;
    
    // Threading for window updates
    std::thread m_updateThread;
    std::mutex m_windowMutex;
    std::atomic<bool> m_stopThread;
    
    // UI state
    int m_selectedIndex;
    int m_scrollOffset;
    std::wstring m_searchText;
    bool m_isCaretVisible;
    
    // GDI objects
    HFONT m_font;
    HBRUSH m_backgroundBrush;
    HBRUSH m_selectedBrush;
    
    // Static window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnKeyDown(WPARAM vkCode, bool isShiftPressed);
    void OnChar(WPARAM ch);
    void OnCustomKeyDown(WPARAM vkCode, LPARAM lParam);
    
    // UI methods
    void SelectNext();
    void SelectPrevious();
    void ActivateSelectedWindow();
    void DrawWindow(HDC hdc);
    void DrawSearchBox(HDC hdc);
    void DrawWindowItem(HDC hdc, const WindowInfo& window, int index, int y);
    void DrawIcon(HDC hdc, HICON icon, int x, int y);
    void DrawTextString(HDC hdc, const std::wstring& text, int x, int y, int width, COLORREF color);
    RECT GetItemRect(int index);

    // Constants
    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"TabSwitcherWindowClass";
    static constexpr const wchar_t* WINDOW_TITLE = L"Tab Switcher";
}; 