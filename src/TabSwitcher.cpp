#include "TabSwitcher.h"
#include <iostream>
#include <string>
#include "Config.h"
#include <windowsx.h>
#include <dwmapi.h> // Include for DWM functions
#include <algorithm>



// Define modern DWM attributes if they are not available in the current SDK
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif


TabSwitcher::TabSwitcher() 
    : m_hwnd(nullptr)
    , m_hThumbnail(nullptr)
    , m_hInstance(GetModuleHandle(nullptr))
    , m_isVisible(false)
    , m_selectedIndex(0)
    , m_scrollOffset(0)
    , m_isCaretVisible(true)
    , m_font(nullptr)
    , m_backgroundBrush(nullptr)
    , m_selectedBrush(nullptr)
    , m_stopThread(false) {
    
    m_windowManager = std::make_unique<WindowManager>();
    RegisterWindowClass();
    StartWindowUpdater();
}

TabSwitcher::~TabSwitcher() {
    StopWindowUpdater();
    UnregisterThumbnail();
    if (m_font) DeleteObject(m_font);
    if (m_backgroundBrush) DeleteObject(m_backgroundBrush);
    if (m_selectedBrush) DeleteObject(m_selectedBrush);
    if (m_hwnd) DestroyWindow(m_hwnd);
    UnregisterWindowClass();
}

bool TabSwitcher::Create() {
    m_font = CreateFontW(
        Config::FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, Config::FONT_NAME.c_str()
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
    if (m_isVisible.load()) return;

    // The window list is now updated in the background.
    // We just need to grab the latest version of it.
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        // m_windows is already up-to-date from the background thread.
    }
    
    m_searchText.clear();
    FilterWindows();
    
    // It's possible the list is empty right at the start
    // if the background thread hasn't populated it yet.
    // The UI will just show "no windows".
    
    m_selectedIndex = 0;
    m_scrollOffset = 0;

    // Apply Mica effect if available (Windows 11+)
    BOOL micaValue = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &micaValue, sizeof(micaValue));

    CenterOnScreen();
    ShowWindow(m_hwnd, SW_SHOWNA); // Show without activating
    SetForegroundWindow(m_hwnd); // Force it to the foreground
    SetFocus(m_hwnd);
    
    m_isVisible.store(true);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::Hide() {
    if (!m_isVisible.load()) return;
    UnregisterThumbnail();
    ShowWindow(m_hwnd, SW_HIDE);
    m_isVisible.store(false);
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

        case WM_APP + 2: // Refresh from background thread
            {
                HWND previouslySelectedHwnd = nullptr;
                if (!m_filteredWindows.empty() && m_selectedIndex < m_filteredWindows.size()) {
                    previouslySelectedHwnd = m_filteredWindows[m_selectedIndex].hwnd;
                }

                FilterWindows(); // Rebuilds the list and resets selection to 0.

                if (previouslySelectedHwnd) {
                    auto it = std::find_if(m_filteredWindows.begin(), m_filteredWindows.end(),
                                           [previouslySelectedHwnd](const WindowInfo& info) {
                                               return info.hwnd == previouslySelectedHwnd;
                                           });

                    if (it != m_filteredWindows.end()) {
                        m_selectedIndex = static_cast<int>(std::distance(m_filteredWindows.begin(), it));
                        EnsureSelectionIsVisible();
                    }
                }
                InvalidateRect(m_hwnd, nullptr, TRUE); // Repaint with the restored selection
            }
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
    if (m_filteredWindows.empty() || m_selectedIndex >= m_filteredWindows.size()) {
        // If there's nothing to show, just paint the default window
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        DrawWindow(hdc);
        EndPaint(m_hwnd, &ps);
        return;
    }

    HWND targetHwnd = m_filteredWindows[m_selectedIndex].hwnd;
    RegisterThumbnail(targetHwnd);

    if (m_hThumbnail) {
        SIZE sourceSize;
        if (SUCCEEDED(DwmQueryThumbnailSourceSize(m_hThumbnail, &sourceSize))) {
            RECT clientRect;
            GetClientRect(m_hwnd, &clientRect);

            float aspectRatio = (float)sourceSize.cy / (float)sourceSize.cx;
            int previewWidth = 250; // Thumbnail width
            int previewHeight = (int)(previewWidth * aspectRatio);

            RECT destRect = {
                clientRect.right + 10, // Position to the right of the main window
                (clientRect.bottom - previewHeight) / 2, // Centered vertically
                clientRect.right + 10 + previewWidth,
                (clientRect.bottom - previewHeight) / 2 + previewHeight
            };

            DWM_THUMBNAIL_PROPERTIES props;
            props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
            props.rcDestination = destRect;
            props.fVisible = TRUE;
            props.opacity = 255;
            DwmUpdateThumbnailProperties(m_hThumbnail, &props);
        }
    }

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
                std::cout << "Shift+Tab pressed - going backward" << std::endl;
                SelectPrevious();
            } else {
                std::cout << "Tab pressed - going forward" << std::endl;
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
                if (ToUnicode(static_cast<UINT>(vkCode), scanCode, keyboardState, buffer, 2, 0) == 1) {
            OnChar(buffer[0]);
        }
    }
}

void TabSwitcher::FilterWindows() {
    std::lock_guard<std::mutex> lock(m_windowMutex);
    m_filteredWindows.clear();
    InvalidateRect(m_hwnd, nullptr, TRUE);

    if (m_searchText.empty()) {
        m_filteredWindows = m_windows;
    } else {
        // For debugging: convert wstring to string for cout
        std::string search_text_str;
        std::transform(m_searchText.begin(), m_searchText.end(), std::back_inserter(search_text_str),
                      [](wchar_t c) { return static_cast<char>(c); });
        std::cout << "Searching for: " << search_text_str << std::endl;

        // Convert search text to lowercase for case-insensitive matching
        std::wstring search_lower = m_searchText;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::towlower);

        for (const auto& window : m_windows) {
            // For debugging: convert wstring to string for cout
            std::string window_title_str;
            std::transform(window.title.begin(), window.title.end(), std::back_inserter(window_title_str),
                          [](wchar_t c) { return static_cast<char>(c); });
            
            std::string process_name_str;
            std::transform(window.processName.begin(), window.processName.end(), std::back_inserter(process_name_str),
                          [](wchar_t c) { return static_cast<char>(c); });

            // Convert window title and process name to lowercase for case-insensitive matching
            std::wstring title_lower = window.title;
            std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::towlower);
            
            std::wstring process_lower = window.processName;
            std::transform(process_lower.begin(), process_lower.end(), process_lower.begin(), ::towlower);

            // Calculate scores for window title
            double title_rapidfuzz = CalculateRapidFuzzScore(search_lower, title_lower);
            double title_position = CalculatePositionScore(search_lower, title_lower);
            double title_prefix = CalculatePrefixScore(search_lower, title_lower);
            double title_sequential = CalculateSequentialScore(search_lower, title_lower);

            double title_score = (title_rapidfuzz * 0.3) + 
                               (title_position * 0.2) + 
                               (title_prefix * 0.3) + 
                               (title_sequential * 0.2);

            // Calculate scores for process name
            double process_rapidfuzz = CalculateRapidFuzzScore(search_lower, process_lower);
            double process_position = CalculatePositionScore(search_lower, process_lower);
            double process_prefix = CalculatePrefixScore(search_lower, process_lower);
            double process_sequential = CalculateSequentialScore(search_lower, process_lower);

            double process_score = (process_rapidfuzz * 0.3) + 
                                 (process_position * 0.2) + 
                                 (process_prefix * 0.3) + 
                                 (process_sequential * 0.2);

            // Take the better score, but give a small bonus if process name matches well
            double final_score = std::max(title_score, process_score);
            
            // Bonus if process name has a good match (helps with app-specific searches)
            if (process_score > 70) {
                final_score += 10; // Small bonus for good process name matches
            }
            
            std::cout << "Window: '" << window_title_str << "' (Process: '" << process_name_str << "')" 
                      << " | Title Score: " << title_score << " | Process Score: " << process_score 
                      << " | Final: " << final_score << std::endl;

            // Use a threshold for quality results
            if (final_score > 60) {
                WindowInfo info = window;
                info.score = final_score;
                m_filteredWindows.push_back(info);
            }
        }

        // Sort by score in descending order
        std::sort(m_filteredWindows.begin(), m_filteredWindows.end(), [](const WindowInfo& a, const WindowInfo& b) {
            return a.score > b.score;
        });
    }
    m_selectedIndex = 0;
    m_scrollOffset = 0;
}

void TabSwitcher::SelectNext() {
    if (m_filteredWindows.empty()) return;
    InvalidateRect(m_hwnd, nullptr, TRUE);
    {
        int oldSelectedIndex = m_selectedIndex;
        int oldScrollOffset = m_scrollOffset;

        m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(m_filteredWindows.size());
        EnsureSelectionIsVisible();

        if (m_scrollOffset != oldScrollOffset) {
            InvalidateRect(m_hwnd, nullptr, TRUE);
        } else {
            RECT oldItemRect = GetItemRect(oldSelectedIndex);
            InvalidateRect(m_hwnd, &oldItemRect, TRUE);
            RECT newItemRect = GetItemRect(m_selectedIndex);
            InvalidateRect(m_hwnd, &newItemRect, TRUE);
        }
    }
}

void TabSwitcher::SelectPrevious() {
    if (m_filteredWindows.empty()) return;
    InvalidateRect(m_hwnd, nullptr, TRUE);
    {
        int oldSelectedIndex = m_selectedIndex;
        int oldScrollOffset = m_scrollOffset;

        m_selectedIndex = (m_selectedIndex - 1 + static_cast<int>(m_filteredWindows.size())) 
                         % static_cast<int>(m_filteredWindows.size());
        EnsureSelectionIsVisible();

        if (m_scrollOffset != oldScrollOffset) {
            InvalidateRect(m_hwnd, nullptr, TRUE);
        } else {
            RECT oldItemRect = GetItemRect(oldSelectedIndex);
            InvalidateRect(m_hwnd, &oldItemRect, TRUE);
            RECT newItemRect = GetItemRect(m_selectedIndex);
            InvalidateRect(m_hwnd, &newItemRect, TRUE);
        }
    }
}

void TabSwitcher::ActivateSelectedWindow() {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_filteredWindows.size())) {
        const WindowInfo& window = m_filteredWindows[m_selectedIndex];

        Hide();
        m_windowManager->ActivateWindow(window.hwnd);
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

RECT TabSwitcher::GetItemRect(int index) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);

    int relativeIndex = index - m_scrollOffset;
    int y = Config::PADDING + Config::ITEM_HEIGHT + (relativeIndex * Config::ITEM_HEIGHT);

    RECT itemRect = {
        Config::PADDING, y,
        clientRect.right - Config::PADDING, y + Config::ITEM_HEIGHT
    };
    return itemRect;
}

void TabSwitcher::CenterOnScreen() {
    Utils::CenterWindow(m_hwnd, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT);
}

void TabSwitcher::RegisterThumbnail(HWND targetHwnd) {
    if (m_hThumbnail) {
        UnregisterThumbnail();
    }

    if (SUCCEEDED(DwmRegisterThumbnail(m_hwnd, targetHwnd, &m_hThumbnail))) {
        // Success
    }
}

void TabSwitcher::UnregisterThumbnail() {
    if (m_hThumbnail) {
        DwmUnregisterThumbnail(m_hThumbnail);
        m_hThumbnail = nullptr;
    }
}

// Improved fuzzy matching scoring methods

double TabSwitcher::CalculateRapidFuzzScore(const std::wstring& search, const std::wstring& target) {
    // Use token_set_ratio for better matching of individual words
    double token_set_score = rapidfuzz::fuzz::token_set_ratio(search, target);
    double partial_score = rapidfuzz::fuzz::partial_ratio(search, target);
    
    // Prefer token_set for better word matching, but use partial as backup
    return std::max(token_set_score, partial_score * 0.8);
}

double TabSwitcher::CalculatePositionScore(const std::wstring& search, const std::wstring& target) {
    if (search.empty() || target.empty()) return 0.0;
    
    double score = 0.0;
    size_t search_len = search.length();
    size_t target_len = target.length();
    
    // Find each character of search in target and calculate position bonus
    size_t last_found_pos = 0;
    double position_penalty = 0.0;
    
    for (size_t i = 0; i < search_len; ++i) {
        size_t found_pos = target.find(search[i], last_found_pos);
        if (found_pos != std::wstring::npos) {
            // Earlier positions get higher scores
            double position_score = 1.0 - (static_cast<double>(found_pos) / target_len);
            score += position_score;
            last_found_pos = found_pos + 1;
        } else {
            // Character not found, apply penalty
            position_penalty += 0.2;
        }
    }
    
    // Normalize score and apply penalty
    score = (score / search_len) * 100.0;
    score = std::max(0.0, score - (position_penalty * 100.0));
    
    return score;
}

double TabSwitcher::CalculatePrefixScore(const std::wstring& search, const std::wstring& target) {
    if (search.empty() || target.empty()) return 0.0;
    
    double score = 0.0;
    
    // Check for exact prefix match
    if (target.find(search) == 0) {
        score = 100.0; // Perfect prefix match
    } else {
        // Check for word-start prefix matches
        size_t pos = 0;
        while ((pos = target.find(L' ', pos)) != std::wstring::npos) {
            pos++; // Move past the space
            if (pos < target.length()) {
                std::wstring word_start = target.substr(pos);
                if (word_start.find(search) == 0) {
                    score = 80.0; // Word start match
                    break;
                }
            }
        }
        
        // Check for character-level prefix matching
        if (score == 0.0) {
            size_t matching_chars = 0;
            size_t min_len = std::min(search.length(), target.length());
            
            for (size_t i = 0; i < min_len; ++i) {
                if (search[i] == target[i]) {
                    matching_chars++;
                } else {
                    break;
                }
            }
            
            if (matching_chars > 0) {
                score = (static_cast<double>(matching_chars) / search.length()) * 60.0;
            }
        }
    }
    
    return score;
}

double TabSwitcher::CalculateSequentialScore(const std::wstring& search, const std::wstring& target) {
    if (search.empty() || target.empty()) return 0.0;
    
    double score = 0.0;
    size_t search_len = search.length();
    size_t target_len = target.length();
    
    // Find longest consecutive character sequences
    size_t max_consecutive = 0;
    size_t current_consecutive = 0;
    size_t total_matches = 0;
    
    size_t search_idx = 0;
    size_t target_idx = 0;
    
    while (search_idx < search_len && target_idx < target_len) {
        if (search[search_idx] == target[target_idx]) {
            current_consecutive++;
            total_matches++;
            search_idx++;
            target_idx++;
            max_consecutive = std::max(max_consecutive, current_consecutive);
        } else {
            current_consecutive = 0;
            target_idx++;
        }
    }
    
    if (total_matches > 0) {
        // Base score from match ratio
        double match_ratio = static_cast<double>(total_matches) / search_len;
        
        // Bonus for consecutive characters
        double consecutive_bonus = static_cast<double>(max_consecutive) / search_len;
        
        // Combined score with extra weight for consecutive matches
        score = (match_ratio * 60.0) + (consecutive_bonus * 40.0);
    }
    
    return score;
}

void TabSwitcher::StartWindowUpdater() {
    m_updateThread = std::thread([this] {
        UpdateWindowsInBackground();
    });
}

void TabSwitcher::StopWindowUpdater() {
    m_stopThread = true;
    if (m_updateThread.joinable()) {
        m_updateThread.join();
    }
}

void TabSwitcher::UpdateWindowsInBackground() {
    while (!m_stopThread) {
        auto newWindows = m_windowManager->GetAllWindows();
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            m_windows = std::move(newWindows);
        }

        // If the window is visible, refresh the filtered list
        if (m_isVisible.load()) {
            PostMessage(m_hwnd, WM_APP + 2, 0, 0); // Custom message to refresh
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
 