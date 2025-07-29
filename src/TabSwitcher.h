#pragma once

#include "Utils.h"
#include "WindowManager.h"
#include "Config.h"
#include <vector>
#include <string>
#include <memory>

constexpr UINT WM_APP_KEYDOWN = WM_APP + 1;

class TabSwitcher {
public:
    TabSwitcher();
    ~TabSwitcher();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const { return m_isVisible; }
    HWND GetHwnd() const { return m_hwnd; }

private:
    void RegisterWindowClass();
    void UnregisterWindowClass();
    void CenterOnScreen();
    void FilterWindows();
    void EnsureSelectionIsVisible();

    // Window management
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    bool m_isVisible;
    
    // Window data
    std::unique_ptr<WindowManager> m_windowManager;
    std::vector<WindowInfo> m_windows;
    std::vector<WindowInfo> m_filteredWindows;
    
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
    
    // Constants
    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"TabSwitcherWindowClass";
    static constexpr const wchar_t* WINDOW_TITLE = L"Tab Switcher";
}; 