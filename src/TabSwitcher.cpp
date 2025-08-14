#include "TabSwitcher.h"
#include <string>
#ifdef DEBUG
#include <iostream>
#endif
#include "Config.h"
#include <windowsx.h>
#include <dwmapi.h> // Include for DWM functions
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <cstdint>

#ifndef algorithm
#include <algorithm>
#endif


// Define modern DWM attributes if they are not available in the current SDK
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif


TabSwitcher* TabSwitcher::s_instance = nullptr;

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

    s_instance = this;
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
    s_instance = nullptr;
}

bool TabSwitcher::Create() {
    m_backgroundBrush = CreateSolidBrush(Config::BG_COLOR);
    m_selectedBrush = CreateSolidBrush(Config::SELECTED_COLOR);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_POPUP,
        0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
        nullptr, nullptr, m_hInstance, this
    );

    if (m_hwnd) {
        UINT dpi = GetDpiForWindow(m_hwnd);
        m_scaleFactor = static_cast<float>(dpi) / 96.0f;

        Config::WINDOW_WIDTH = static_cast<int>(Config::WINDOW_WIDTH * m_scaleFactor);
        Config::WINDOW_HEIGHT = static_cast<int>(Config::WINDOW_HEIGHT * m_scaleFactor);
        Config::ITEM_HEIGHT = static_cast<int>(Config::ITEM_HEIGHT * m_scaleFactor);
        Config::PADDING = static_cast<int>(Config::PADDING * m_scaleFactor);
        Config::ICON_SIZE = static_cast<int>(Config::ICON_SIZE * m_scaleFactor);
        Config::PREVIEW_WIDTH = static_cast<int>(Config::PREVIEW_WIDTH * m_scaleFactor);
        Config::FONT_SIZE = static_cast<int>(Config::FONT_SIZE * m_scaleFactor);

        SetWindowPos(m_hwnd, nullptr, 0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        m_font = CreateFontW(
            Config::FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, Config::FONT_NAME.c_str()
        );

        BOOL enable = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &enable, sizeof(enable));
        SetLayeredWindowAttributes(m_hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    }

    return m_hwnd != nullptr;
}

void TabSwitcher::Show() {
    if (m_isVisible.load()) return;

    // Use the existing window list; if it's empty, populate it once
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        if (m_windows.empty()) {
            m_windows = m_windowManager->GetAllWindows();
        }
    }

    // Ask the background thread to refresh immediately if polling is active
    if (m_useFallback) {
        m_updateCv.notify_one();
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

    // Start caret blinking timer
    SetTimer(m_hwnd, 1, 500, nullptr); // Timer ID 1, 500ms interval

    m_isVisible.store(true);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::Hide() {
    if (!m_isVisible.load()) return;
    UnregisterThumbnail();
    ShowWindow(m_hwnd, SW_HIDE);
    KillTimer(m_hwnd, 1); // Stop caret timer
    m_isVisible.store(false);
}

void TabSwitcher::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_backgroundBrush; // Use the member brush
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

        case WM_ERASEBKGND:
            return OnEraseBkgnd((HDC)wParam);

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

        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDBLCLK:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            ActivateSelectedWindow();
            return 0;

        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
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
        UnregisterThumbnail();
    } else {
        HWND targetHwnd = m_filteredWindows[m_selectedIndex].hwnd;
        RegisterThumbnail(targetHwnd);

        if (m_hThumbnail) {
            SIZE sourceSize;
            if (SUCCEEDED(DwmQueryThumbnailSourceSize(m_hThumbnail, &sourceSize))) {
                RECT clientRect;
                GetClientRect(m_hwnd, &clientRect);

                float aspectRatio = (float)sourceSize.cy / (float)sourceSize.cx;
                int previewWidth = Config::PREVIEW_WIDTH;
                int previewHeight = static_cast<int>(previewWidth * aspectRatio);

                int previewLeft = clientRect.right - Config::PREVIEW_WIDTH - Config::PADDING;
                RECT destRect = {
                    previewLeft,
                    (clientRect.bottom - previewHeight) / 2,
                    previewLeft + previewWidth,
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
    }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    DrawWindow(hdc);
    EndPaint(m_hwnd, &ps);
}

LRESULT TabSwitcher::OnEraseBkgnd(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    FillRect(hdc, &clientRect, m_backgroundBrush);
    return 1; // We've handled erasing the background.
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

        case VK_PRIOR: // Page Up
            SelectPageUp();
            break;

        case VK_NEXT: // Page Down
            SelectPageDown();
            break;

        case VK_HOME:
            SelectFirst();
            break;

        case VK_END:
            SelectLast();
            break;

        case VK_TAB:
            if (isShiftPressed) {
#ifdef DEBUG
                std::cout << "Shift+Tab pressed - going backward" << std::endl;
#endif
                SelectPrevious();
            } else {
#ifdef DEBUG
                std::cout << "Tab pressed - going forward" << std::endl;
#endif
                SelectNext();
            }
            break;
            
        case VK_BACK:
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (!m_searchText.empty()) {
                    while (!m_searchText.empty() && m_searchText.back() == L' ')
                        m_searchText.pop_back();
                    while (!m_searchText.empty() && m_searchText.back() != L' ')
                        m_searchText.pop_back();
                    FilterWindows();
                    InvalidateRect(m_hwnd, nullptr, TRUE);
                }
            } else if (!m_searchText.empty()) {
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
    if (vkCode == VK_ESCAPE || vkCode == VK_RETURN || vkCode == VK_UP || vkCode == VK_DOWN ||
        vkCode == VK_BACK || vkCode == VK_TAB || vkCode == VK_PRIOR || vkCode == VK_NEXT ||
        vkCode == VK_HOME || vkCode == VK_END) {
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

void TabSwitcher::OnLButtonDown(int x, int y) {
    (void)x;
    int listTop = Config::PADDING + Config::ITEM_HEIGHT;
    if (y < listTop) {
        return; // Clicked in search box or padding
    }

    int index = (y - listTop) / Config::ITEM_HEIGHT + m_scrollOffset;
    if (index < 0 || index >= static_cast<int>(m_filteredWindows.size())) {
        return;
    }

    int oldSelectedIndex = m_selectedIndex;
    m_selectedIndex = index;
    EnsureSelectionIsVisible();
    SetFocus(m_hwnd);

    if (oldSelectedIndex != m_selectedIndex) {
        if (oldSelectedIndex >= 0 && oldSelectedIndex < static_cast<int>(m_filteredWindows.size())) {
            RECT oldRect = GetItemRect(oldSelectedIndex);
            InvalidateRect(m_hwnd, &oldRect, TRUE);
        }
        RECT newRect = GetItemRect(m_selectedIndex);
        InvalidateRect(m_hwnd, &newRect, TRUE);
    }
}

void TabSwitcher::OnMouseWheel(short delta) {
    if (delta > 0) {
        SelectPrevious();
    } else if (delta < 0) {
        SelectNext();
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
#ifdef DEBUG
        std::string search_text_str;
        std::transform(m_searchText.begin(), m_searchText.end(), std::back_inserter(search_text_str),
                       [](wchar_t c) { return static_cast<char>(c); });
        std::cout << "Searching for: " << search_text_str << std::endl;
#endif

        // Convert search text to lowercase for case-insensitive matching
        std::wstring search_lower = m_searchText;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::towlower);

        bool useFuzzy = search_lower.size() <= 2;

        for (const auto& window : m_windows) {
#ifdef DEBUG
            std::string window_title_str;
            std::transform(window.title.begin(), window.title.end(), std::back_inserter(window_title_str),
                           [](wchar_t c) { return static_cast<char>(c); });

            std::string process_name_str;
            std::transform(window.processName.begin(), window.processName.end(), std::back_inserter(process_name_str),
                           [](wchar_t c) { return static_cast<char>(c); });
#endif

            std::wstring title_lower = window.title;
            std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::towlower);

            std::wstring process_lower = Utils::RemoveFileExtension(window.processName);
            std::transform(process_lower.begin(), process_lower.end(), process_lower.begin(), ::towlower);

            auto score_fn = [&](const std::wstring& target) -> double {
                if (target.rfind(search_lower, 0) == 0)
                    return 100.0;
                if (target.find(search_lower) != std::wstring::npos)
                    return 80.0;
                if (useFuzzy && BitapSearch(target, search_lower))
                    return 60.0;
                return 0.0;
            };

            double title_score = score_fn(title_lower);
            double process_score = score_fn(process_lower);
            double final_score = std::max(title_score, process_score);

#ifdef DEBUG
            std::cout << "Window: '" << window_title_str << "' (Process: '" << process_name_str << "')"
                      << " | Title Score: " << title_score << " | Process Score: " << process_score
                      << " | Final: " << final_score << std::endl;
#endif

            if (final_score > 50) {
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

void TabSwitcher::SelectPageDown() {
    if (m_filteredWindows.empty()) return;
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int listTopY = Config::PADDING + Config::ITEM_HEIGHT;
    int maxVisibleItems = (clientRect.bottom - listTopY) / Config::ITEM_HEIGHT;

    m_selectedIndex = std::min(m_selectedIndex + maxVisibleItems,
                               static_cast<int>(m_filteredWindows.size()) - 1);
    EnsureSelectionIsVisible();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::SelectPageUp() {
    if (m_filteredWindows.empty()) return;
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int listTopY = Config::PADDING + Config::ITEM_HEIGHT;
    int maxVisibleItems = (clientRect.bottom - listTopY) / Config::ITEM_HEIGHT;

    m_selectedIndex = std::max(m_selectedIndex - maxVisibleItems, 0);
    EnsureSelectionIsVisible();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::SelectFirst() {
    if (m_filteredWindows.empty()) return;
    m_selectedIndex = 0;
    EnsureSelectionIsVisible();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void TabSwitcher::SelectLast() {
    if (m_filteredWindows.empty()) return;
    m_selectedIndex = static_cast<int>(m_filteredWindows.size()) - 1;
    EnsureSelectionIsVisible();
    InvalidateRect(m_hwnd, nullptr, TRUE);
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
    int previewLeft = clientRect.right - Config::PREVIEW_WIDTH - Config::PADDING;

    // The background is now handled by DWM (Mica/Acrylic), so we don't need to fill it.
    // FillRect(hdc, &clientRect, m_backgroundBrush);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, m_font);
    SetBkMode(hdc, TRANSPARENT);
    
    DrawSearchBox(hdc);

    int y = Config::PADDING + Config::ITEM_HEIGHT; // Start list below search box
    int textHeight = static_cast<int>(20 * m_scaleFactor);

    if (m_filteredWindows.empty()) {
        bool windowsEmpty;
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            windowsEmpty = m_windows.empty();
        }
        std::wstring message = windowsEmpty ? L"Lade Fenster..." : L"Keine Fenster gefunden";
        DrawTextString(hdc, message, Config::PADDING,
                       y + (Config::ITEM_HEIGHT - textHeight) / 2,
                       previewLeft - 2 * Config::PADDING, Config::TEXT_COLOR);
        SelectObject(hdc, oldFont);
        return;
    }

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
    int previewLeft = clientRect.right - Config::PREVIEW_WIDTH - Config::PADDING;

    RECT searchRect = {
        Config::PADDING, Config::PADDING,
        previewLeft - Config::PADDING, Config::PADDING + Config::ITEM_HEIGHT
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
    int textHeight = static_cast<int>(20 * m_scaleFactor);

    std::wstring displayText = L"Search: " + m_searchText;

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, displayText.c_str(), static_cast<int>(displayText.length()), &textSize);

    int availableWidth = searchRect.right - x - Config::PADDING;
    int scrollOffset = 0;

    if (textSize.cx > availableWidth) {
        scrollOffset = textSize.cx - availableWidth;
    }

    int savedDC = SaveDC(hdc);
    RECT clipRect = { x, searchRect.top, searchRect.right - Config::PADDING, searchRect.bottom };
    IntersectClipRect(hdc, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Config::TEXT_COLOR);
    TextOutW(hdc, x - scrollOffset, Config::PADDING + (Config::ITEM_HEIGHT - textHeight) / 2,
             displayText.c_str(), static_cast<int>(displayText.length()));
    RestoreDC(hdc, savedDC);

    if (m_isCaretVisible) {
        int caretX = x + textSize.cx - scrollOffset;
        caretX = std::min(caretX, static_cast<int>(searchRect.right - Config::PADDING));
        int caretY = searchRect.top + (Config::ITEM_HEIGHT - textHeight) / 2;
        int caretWidth = std::max(1, static_cast<int>(2 * m_scaleFactor));
        RECT caretRect = { caretX, caretY, caretX + caretWidth, caretY + textHeight };
        FillRect(hdc, &caretRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }
}


void TabSwitcher::DrawWindowItem(HDC hdc, const WindowInfo& window, int index, int y) {
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int previewLeft = clientRect.right - Config::PREVIEW_WIDTH - Config::PADDING;

    RECT itemRect = {
        Config::PADDING, y,
        previewLeft - Config::PADDING, y + Config::ITEM_HEIGHT
    };
    
    // Draw a custom selection cursor instead of filling the whole item
    if (index == m_selectedIndex) {
        HPEN highlightPen = CreatePen(PS_SOLID, std::max(1, static_cast<int>(2 * m_scaleFactor)), Config::HIGHLIGHT_COLOR);

        HPEN oldPen = (HPEN)SelectObject(hdc, highlightPen);
        int cursorY = y + Config::ITEM_HEIGHT / 2;
        int cursorOffset = static_cast<int>(5 * m_scaleFactor);
        int cursorX = itemRect.left + cursorOffset;
        MoveToEx(hdc, cursorX, cursorY - cursorOffset, nullptr);
        LineTo(hdc, cursorX + cursorOffset, cursorY);
        LineTo(hdc, cursorX, cursorY + cursorOffset);
        SelectObject(hdc, oldPen);

        DeleteObject(highlightPen);
    }

    int x = itemRect.left + Config::PADDING + static_cast<int>(15 * m_scaleFactor); // Indent text a bit more
    
    if (window.icon) {
        DrawIconEx(hdc, x, y + (Config::ITEM_HEIGHT - Config::ICON_SIZE) / 2, window.icon, 
                   Config::ICON_SIZE, Config::ICON_SIZE, 0, nullptr, DI_NORMAL);
    }
    x += Config::ICON_SIZE + Config::PADDING;
    
    std::wstring displayText = window.title;

    COLORREF textColor = Config::TEXT_COLOR; // Text color is now consistent
    int textHeight = static_cast<int>(20 * m_scaleFactor);
    int textY = y + (Config::ITEM_HEIGHT - textHeight) / 2;
    int maxWidth = itemRect.right - x - Config::PADDING;

    int savedDC = SaveDC(hdc);
    RECT clipRect = { x, textY, x + maxWidth, textY + textHeight };
    IntersectClipRect(hdc, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);

    if (m_searchText.empty()) {
        SetTextColor(hdc, textColor);
        TextOutW(hdc, x, textY, displayText.c_str(), static_cast<int>(displayText.length()));
    } else {
        std::wstring titleLower = displayText;
        std::wstring searchLower = m_searchText;
        std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::towlower);
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::towlower);

        size_t pos = 0;
        int curX = x;
        while (pos < displayText.size()) {
            size_t matchPos = titleLower.find(searchLower, pos);
            size_t segmentEnd = matchPos == std::wstring::npos ? displayText.size() : matchPos;

            std::wstring segment = displayText.substr(pos, segmentEnd - pos);
            if (!segment.empty()) {
                SIZE size{};
                GetTextExtentPoint32W(hdc, segment.c_str(), static_cast<int>(segment.size()), &size);
                SetTextColor(hdc, textColor);
                TextOutW(hdc, curX, textY, segment.c_str(), static_cast<int>(segment.size()));
                curX += size.cx;
            }

            if (matchPos == std::wstring::npos) {
                break;
            }

            std::wstring matchSegment = displayText.substr(matchPos, m_searchText.size());
            if (!matchSegment.empty()) {
                SIZE size{};
                GetTextExtentPoint32W(hdc, matchSegment.c_str(), static_cast<int>(matchSegment.size()), &size);
                SetTextColor(hdc, Config::HIGHLIGHT_COLOR);
                TextOutW(hdc, curX, textY, matchSegment.c_str(), static_cast<int>(matchSegment.size()));
                curX += size.cx;
            }

            pos = matchPos + m_searchText.size();
        }
    }

    RestoreDC(hdc, savedDC);
}

void TabSwitcher::DrawIcon(HDC hdc, HICON icon, int x, int y) {
    DrawIconEx(hdc, x, y, icon, Config::ICON_SIZE, Config::ICON_SIZE, 0, nullptr, DI_NORMAL);
}

void TabSwitcher::DrawTextString(HDC hdc, const std::wstring& text, int x, int y, int width, COLORREF color) {
    SetTextColor(hdc, color);
    int textHeight = static_cast<int>(20 * m_scaleFactor);
    RECT textRect = { x, y, x + width, y + textHeight };
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
    int previewLeft = clientRect.right - Config::PREVIEW_WIDTH - Config::PADDING;

    int relativeIndex = index - m_scrollOffset;
    int y = Config::PADDING + Config::ITEM_HEIGHT + (relativeIndex * Config::ITEM_HEIGHT);

    RECT itemRect = {
        Config::PADDING, y,
        previewLeft - Config::PADDING, y + Config::ITEM_HEIGHT
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

// Bitap/Shift-And search allowing a small number of errors
bool TabSwitcher::BitapSearch(const std::wstring& text, const std::wstring& pattern, int maxErrors) {
    if (pattern.empty()) return true;
    if (pattern.size() > 63) {
        return text.find(pattern) != std::wstring::npos;
    }

    std::unordered_map<wchar_t, uint64_t> mask;
    for (size_t i = 0; i < pattern.size(); ++i) {
        mask[pattern[i]] |= (1ULL << i);
    }

    std::vector<uint64_t> R(maxErrors + 1, ~0ULL);
    uint64_t matchMask = 1ULL << (pattern.size() - 1);

    for (wchar_t c : text) {
        uint64_t charMask = mask.count(c) ? mask[c] : 0;
        uint64_t oldR = R[0];
        R[0] = ((R[0] << 1) | 1ULL) & charMask;
        for (int d = 1; d <= maxErrors; ++d) {
            uint64_t tmp = R[d];
            R[d] = ((R[d] << 1) | 1ULL) & charMask;
            R[d] |= (oldR << 1) | oldR;
            oldR = tmp;
        }
        if (R[maxErrors] & matchMask) {
            return true;
        }
    }
    return false;
}

void TabSwitcher::StartWindowUpdater() {
    {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        m_windows = m_windowManager->GetAllWindows();
    }

    if (!RegisterEventHooks()) {
        m_useFallback = true;
        m_updateThread = std::thread([this] { UpdateWindowsInBackground(); });
    }
}

void TabSwitcher::StopWindowUpdater() {
    if (m_useFallback) {
        m_stopThread = true;
        m_updateCv.notify_all();
        if (m_updateThread.joinable()) {
            m_updateThread.join();
        }
    } else {
        UnregisterEventHooks();
    }
}

void TabSwitcher::UpdateWindowsInBackground() {
    std::unique_lock<std::mutex> cvLock(m_updateCvMutex);
    size_t lastCount = 0;
    while (!m_stopThread) {
        cvLock.unlock();
        auto newWindows = m_windowManager->GetAllWindows();
        if (newWindows.size() != lastCount) {
            {
                std::lock_guard<std::mutex> lock(m_windowMutex);
                m_windows = std::move(newWindows);
                lastCount = m_windows.size();
            }

            // If the window is visible, refresh the filtered list
            if (m_isVisible.load()) {
                PostMessage(m_hwnd, WM_APP + 2, 0, 0); // Custom message to refresh
            }
        }

        cvLock.lock();
        m_updateCv.wait_for(cvLock, std::chrono::seconds(5));
    }
}

bool TabSwitcher::RegisterEventHooks() {
    DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    m_hookCreate = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr, WinEventProc, 0, 0, flags);
    m_hookDestroy = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, WinEventProc, 0, 0, flags);
    m_hookForeground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, WinEventProc, 0, 0, flags);

    if (m_hookCreate && m_hookDestroy && m_hookForeground) {
        return true;
    }

    UnregisterEventHooks();
    return false;
}

void TabSwitcher::UnregisterEventHooks() {
    if (m_hookCreate) {
        UnhookWinEvent(m_hookCreate);
        m_hookCreate = nullptr;
    }
    if (m_hookDestroy) {
        UnhookWinEvent(m_hookDestroy);
        m_hookDestroy = nullptr;
    }
    if (m_hookForeground) {
        UnhookWinEvent(m_hookForeground);
        m_hookForeground = nullptr;
    }
}

void CALLBACK TabSwitcher::WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                       DWORD, DWORD) {
    if (s_instance) {
        s_instance->HandleWinEvent(event, hwnd, idObject, idChild);
    }
}

void TabSwitcher::HandleWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) {
    if (idObject != OBJID_WINDOW || hwnd == nullptr) {
        return;
    }

    if (!m_windowManager->ShouldIncludeWindow(hwnd)) {
        if (event == EVENT_OBJECT_DESTROY) {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            m_windows.erase(std::remove_if(m_windows.begin(), m_windows.end(),
                                           [hwnd](const WindowInfo& w) { return w.hwnd == hwnd; }),
                            m_windows.end());
        }
        return;
    }

    switch (event) {
    case EVENT_OBJECT_CREATE: {
        WindowInfo info = m_windowManager->CreateWindowInfo(hwnd);
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            auto it = std::find_if(m_windows.begin(), m_windows.end(),
                                   [hwnd](const WindowInfo& w) { return w.hwnd == hwnd; });
            if (it == m_windows.end()) {
                m_windows.push_back(info);
            } else {
                *it = info;
            }
        }
        break;
    }
    case EVENT_OBJECT_DESTROY: {
        std::lock_guard<std::mutex> lock(m_windowMutex);
        m_windows.erase(std::remove_if(m_windows.begin(), m_windows.end(),
                                       [hwnd](const WindowInfo& w) { return w.hwnd == hwnd; }),
                        m_windows.end());
        break;
    }
    case EVENT_SYSTEM_FOREGROUND: {
        WindowInfo info = m_windowManager->CreateWindowInfo(hwnd);
        {
            std::lock_guard<std::mutex> lock(m_windowMutex);
            auto it = std::find_if(m_windows.begin(), m_windows.end(),
                                   [hwnd](const WindowInfo& w) { return w.hwnd == hwnd; });
            if (it == m_windows.end()) {
                m_windows.push_back(info);
            } else {
                *it = info;
            }
        }
        break;
    }
    default:
        break;
    }

    if (m_isVisible.load()) {
        PostMessage(m_hwnd, WM_APP + 2, 0, 0);
    }
}
 