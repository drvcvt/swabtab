#include "TabSwitcher.h"
#include "Config.h"
#include "IPC.h"
#include <windowsx.h>
#include <dwmapi.h> // Include for DWM functions
#include <algorithm>
#include <regex>

// Define modern DWM attributes if they are not available in the current SDK
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif


TabSwitcher::TabSwitcher() 
    : m_hwnd(nullptr)
    , m_hInstance(GetModuleHandle(nullptr))
    , m_isVisible(false)
    , m_selectedIndex(0)
    , m_scrollOffset(0)
    , m_isCaretVisible(true)
    , m_font(nullptr)
    , m_backgroundBrush(nullptr)
    , m_selectedBrush(nullptr) {
    
    m_windowManager = std::make_unique<WindowManager>();
    RegisterWindowClass();
}

TabSwitcher::~TabSwitcher() {
    if (m_font) DeleteObject(m_font);
    if (m_backgroundBrush) DeleteObject(m_backgroundBrush);
    if (m_selectedBrush) DeleteObject(m_selectedBrush);
    if (m_hwnd) DestroyWindow(m_hwnd);
    UnregisterWindowClass();
}

bool TabSwitcher::Create() {
    m_font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    m_backgroundBrush = CreateSolidBrush(Config::BG_COLOR);
    m_selectedBrush = CreateSolidBrush(Config::SELECTED_COLOR);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, // WS_EX_LAYERED for transparency
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_POPUP, // Use WS_POPUP for a borderless window
        0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
        nullptr, nullptr, m_hInstance, this
    );

    if (m_hwnd) {
        // Apply modern styles
        BOOL enable = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &enable, sizeof(enable));

        // Set background to transparent to let the DWM blur through
        SetLayeredWindowAttributes(m_hwnd, RGB(0,0,0), 0, LWA_COLORKEY);

        // Create a timer for the caret
        SetTimer(m_hwnd, 1, 500, nullptr); // Timer ID 1, 500ms interval
    }

    return m_hwnd != nullptr;
}

void TabSwitcher::Show() {
    if (m_isVisible) return;

    m_windows = m_windowManager->GetAllWindows();
    m_searchText.clear();
    FilterWindows();
    
    if (m_filteredWindows.empty()) return;

    m_selectedIndex = 0;
    m_scrollOffset = 0;

    // Apply Mica effect if available (Windows 11+)
    BOOL micaValue = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));


    CenterOnScreen();
    ShowWindow(m_hwnd, SW_SHOWNA); // Show without activating
    SetForegroundWindow(m_hwnd); // Force it to the foreground
    SetFocus(m_hwnd);
    
    m_isVisible = true;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::Hide() {
    if (!m_isVisible) return;
    ShowWindow(m_hwnd, SW_HIDE);
    m_isVisible = false;
}

void TabSwitcher::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(Config::BG_COLOR);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExW(&wc);
}

void TabSwitcher::UnregisterWindowClass() {
    UnregisterClassW(WINDOW_CLASS_NAME, m_hInstance);
}


LRESULT CALLBACK TabSwitcher::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TabSwitcher* switcher = nullptr;
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        switcher = reinterpret_cast<TabSwitcher*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(switcher));
    } else {
        switcher = reinterpret_cast<TabSwitcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (switcher) {
        return switcher->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT TabSwitcher::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_TIMER:
            if (wParam == 1) { // Our caret timer
                m_isCaretVisible = !m_isCaretVisible;
                // Only invalidate the search box rect to avoid flickering
                RECT clientRect;
                GetClientRect(m_hwnd, &clientRect);
                RECT searchRect = {
                    Config::PADDING, Config::PADDING,
                    clientRect.right - Config::PADDING, Config::PADDING + Config::ITEM_HEIGHT
                };
                InvalidateRect(m_hwnd, &searchRect, FALSE);
            }
            return 0;

        case WM_ACTIVATE:
            // Redraw on activation to re-apply DWM effects if needed
            InvalidateRect(m_hwnd, nullptr, TRUE);
            return 0;

        case WM_KEYDOWN:
            OnKeyDown(wParam, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;

        case WM_CHAR:
            OnChar(wParam);
            return 0;

        case WM_APP_KEYDOWN:
            OnCustomKeyDown(wParam, lParam);
            return 0;

        case WM_KILLFOCUS:
            Hide();
            return 0;

        case WM_CLOSE:
            DestroyWindow(m_hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(m_hwnd, uMsg, wParam, lParam);
    }
}

void TabSwitcher::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    DrawWindow(hdc);
    EndPaint(m_hwnd, &ps);
}

void TabSwitcher::OnKeyDown(WPARAM vkCode, bool isShiftPressed) {
    switch (vkCode) {
        case VK_ESCAPE:
            Hide();
            break;

        case VK_RETURN:
            ActivateSelectedWindow();
            break;

        case VK_UP:
            SelectPrevious();
            break;

        case VK_DOWN:
            SelectNext();
            break;

        case VK_TAB:
            if (isShiftPressed) {
                SelectPrevious();
            } else {
                SelectNext();
            }
            break;
            
        case VK_BACK:
            if (!m_searchText.empty()) {
                m_searchText.pop_back();
                FilterWindows();
                InvalidateRect(m_hwnd, nullptr, TRUE);
            }
            break;
    }
}

void TabSwitcher::OnChar(WPARAM ch) {
    if (ch >= 32) { // Printable characters
        m_searchText += static_cast<wchar_t>(ch);
        FilterWindows();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void TabSwitcher::OnCustomKeyDown(WPARAM vkCode, LPARAM lParam) {
    bool isShiftPressed = HIWORD(lParam) != 0;
    UINT scanCode = LOWORD(lParam);

    // Treat as regular key down
    if (vkCode == VK_ESCAPE || vkCode == VK_RETURN || vkCode == VK_UP || vkCode == VK_DOWN || vkCode == VK_BACK || vkCode == VK_TAB) {
        OnKeyDown(vkCode, isShiftPressed);
    } else {
        // For character input, we need to translate the key
        BYTE keyboardState[256];
        GetKeyboardState(keyboardState);
        // Set shift state for character translation
        if (isShiftPressed) {
            keyboardState[VK_SHIFT] |= 0x80;
        } else {
            keyboardState[VK_SHIFT] &= ~0x80;
        }
        
        WCHAR buffer[2];
        if (ToUnicode(vkCode, scanCode, keyboardState, buffer, 2, 0) == 1) {
            OnChar(buffer[0]);
        }
    }
}

void TabSwitcher::FilterWindows() {
    m_filteredWindows.clear();
    if (m_searchText.empty()) {
        m_filteredWindows = m_windows;
    } else {
        try {
            std::wregex searchRegex(m_searchText, std::regex_constants::icase);
            for (const auto& window : m_windows) {
                if (std::regex_search(window.title, searchRegex)) {
                    m_filteredWindows.push_back(window);
                }
            }
        } catch (const std::regex_error&) {
            // Invalid regex, treat as plain text search
            std::wstring lowerSearchText = m_searchText;
            std::transform(lowerSearchText.begin(), lowerSearchText.end(), lowerSearchText.begin(), ::towlower);
            for (const auto& window : m_windows) {
                std::wstring lowerTitle = window.title;
                std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
                if (lowerTitle.find(lowerSearchText) != std::wstring::npos) {
                    m_filteredWindows.push_back(window);
                }
            }
        }
    }
    m_selectedIndex = 0;
    m_scrollOffset = 0;
}

void TabSwitcher::SelectNext() {
    if (!m_filteredWindows.empty()) {
        m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(m_filteredWindows.size());
        EnsureSelectionIsVisible();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void TabSwitcher::SelectPrevious() {
    if (!m_filteredWindows.empty()) {
        m_selectedIndex = (m_selectedIndex - 1 + static_cast<int>(m_filteredWindows.size())) 
                         % static_cast<int>(m_filteredWindows.size());
        EnsureSelectionIsVisible();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void TabSwitcher::ActivateSelectedWindow() {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_filteredWindows.size())) {
        const WindowInfo& window = m_filteredWindows[m_selectedIndex];

        Hide();
        m_windowManager->ActivateWindow(window.hwnd);
        
        SendWindowSelectionToWintile(window);
    }
}

void TabSwitcher::DrawWindow(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    
    // The background is now handled by DWM (Mica/Acrylic), so we don't need to fill it.
    // FillRect(hdc, &clientRect, m_backgroundBrush);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, m_font);
    SetBkMode(hdc, TRANSPARENT);
    
    DrawSearchBox(hdc);

    int y = Config::PADDING + Config::ITEM_HEIGHT; // Start list below search box
    
    int maxVisibleItems = (clientRect.bottom - y) / Config::ITEM_HEIGHT;

    for (int i = 0; i < maxVisibleItems; ++i) {
        int itemIndex = m_scrollOffset + i;
        if (itemIndex >= static_cast<int>(m_filteredWindows.size())) {
            break;
        }
        
        DrawWindowItem(hdc, m_filteredWindows[itemIndex], itemIndex, y);
        y += Config::ITEM_HEIGHT;
    }
    
    SelectObject(hdc, oldFont);
}

void TabSwitcher::DrawSearchBox(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    RECT searchRect = {
        Config::PADDING, Config::PADDING,
        clientRect.right - Config::PADDING, Config::PADDING + Config::ITEM_HEIGHT
    };
    
    // A more subtle background for the search box
    HBRUSH searchBrush = CreateSolidBrush(RGB(40, 40, 40)); 
    FillRect(hdc, &searchRect, searchBrush);
    DeleteObject(searchBrush);

    // Bottom border for the search box
    HPEN borderPen = CreatePen(PS_SOLID, 1, Config::BORDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, searchRect.left, searchRect.bottom, nullptr);
    LineTo(hdc, searchRect.right, searchRect.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    int x = searchRect.left + Config::PADDING;

    std::wstring displayText = L"Search: " + m_searchText;
    
    // Use the primary text color for the search text label
    DrawTextString(hdc, displayText, x, Config::PADDING + (Config::ITEM_HEIGHT - 20) / 2,
                   searchRect.right - x - Config::PADDING, Config::TEXT_COLOR);

    // Draw the blinking caret
    if (m_isCaretVisible) {
        SIZE textSize;
        GetTextExtentPoint32W(hdc, displayText.c_str(), static_cast<int>(displayText.length()), &textSize);
        int caretX = x + textSize.cx;
        int caretY = searchRect.top + (Config::ITEM_HEIGHT - 20) / 2;
        RECT caretRect = { caretX, caretY, caretX + 2, caretY + 20 };
        FillRect(hdc, &caretRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }
}


void TabSwitcher::DrawWindowItem(HDC hdc, const WindowInfo& window, int index, int y) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    
    RECT itemRect = {
        Config::PADDING, y,
        clientRect.right - Config::PADDING, y + Config::ITEM_HEIGHT
    };
    
    // Draw a custom selection cursor instead of filling the whole item
    if (index == m_selectedIndex) {
        //FillRect(hdc, &itemRect, m_selectedBrush);
        HPEN highlightPen = CreatePen(PS_SOLID, 2, Config::HIGHLIGHT_COLOR);
        //FrameRect(hdc, &itemRect, (HBRUSH)highlightPen);
        
        // Draw a ">" like cursor
        HPEN oldPen = (HPEN)SelectObject(hdc, highlightPen);
        int cursorY = y + Config::ITEM_HEIGHT / 2;
        int cursorX = itemRect.left + 5;
        MoveToEx(hdc, cursorX, cursorY - 5, nullptr);
        LineTo(hdc, cursorX + 5, cursorY);
        LineTo(hdc, cursorX, cursorY + 5);
        SelectObject(hdc, oldPen);

        DeleteObject(highlightPen);
    }
    
    int x = itemRect.left + Config::PADDING + 15; // Indent text a bit more
    
    if (window.icon) {
        DrawIconEx(hdc, x, y + (Config::ITEM_HEIGHT - Config::ICON_SIZE) / 2, window.icon, 
                   Config::ICON_SIZE, Config::ICON_SIZE, 0, nullptr, DI_NORMAL);
    }
    x += Config::ICON_SIZE + Config::PADDING;
    
    std::wstring displayText = window.title;
    
    COLORREF textColor = Config::TEXT_COLOR; // Text color is now consistent
    DrawTextString(hdc, displayText, x, y + (Config::ITEM_HEIGHT - 20) / 2,
             itemRect.right - x - Config::PADDING, textColor);
}

void TabSwitcher::DrawIcon(HDC hdc, HICON icon, int x, int y) {
    DrawIconEx(hdc, x, y, icon, Config::ICON_SIZE, Config::ICON_SIZE, 0, nullptr, DI_NORMAL);
}

void TabSwitcher::DrawTextString(HDC hdc, const std::wstring& text, int x, int y, int width, COLORREF color) {
    SetTextColor(hdc, color);
    RECT textRect = { x, y, x + width, y + 20 };
    DrawTextW(hdc, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP);
}

void TabSwitcher::EnsureSelectionIsVisible() {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    int listTopY = Config::PADDING + Config::ITEM_HEIGHT;
    int maxVisibleItems = (clientRect.bottom - listTopY) / Config::ITEM_HEIGHT;

    if (m_selectedIndex < m_scrollOffset) {
        m_scrollOffset = m_selectedIndex;
    } else if (m_selectedIndex >= m_scrollOffset + maxVisibleItems) {
        m_scrollOffset = m_selectedIndex - maxVisibleItems + 1;
    }
}

void TabSwitcher::CenterOnScreen() {
    Utils::CenterWindow(m_hwnd, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
}

void TabSwitcher::SendWindowSelectionToWintile(const WindowInfo& window) {
    if (!IsWintileRunning()) return;
    
    HWND wintileWindow = FindWindowW(WINTILE_WINDOW_CLASS, nullptr);
    if (wintileWindow) {
        WindowSelectionData data = {};
        data.selectedWindow = window.hwnd;
        data.processId = window.processId;
        wcsncpy_s(data.title, window.title.c_str(), _TRUNCATE);
        wcsncpy_s(data.className, window.className.c_str(), _TRUNCATE);
        
        COPYDATASTRUCT copyData = {};
        copyData.dwData = WM_SWABTAB_WINDOW_SELECTED;
        copyData.cbData = sizeof(WindowSelectionData);
        copyData.lpData = &data;
        
        SendMessage(wintileWindow, WM_COPYDATA, 
                   reinterpret_cast<WPARAM>(m_hwnd), 
                   reinterpret_cast<LPARAM>(&copyData));
    }
}

bool TabSwitcher::IsWintileRunning() {
    return FindWindowW(WINTILE_WINDOW_CLASS, nullptr) != nullptr;
}   