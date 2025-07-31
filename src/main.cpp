#include "TabSwitcher.h"
#include "Utils.h"
#include <windows.h>
#include <memory>

std::unique_ptr<TabSwitcher> g_switcher;
HHOOK g_keyboardHook = nullptr;
UINT g_hotkey = VK_RSHIFT; // Default hotkey

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pkbhs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Check for hotkey press to show the switcher
            if (pkbhs->vkCode == g_hotkey) {
                if (g_switcher && !g_switcher->IsVisible()) {
                    g_switcher->Show();
                    return 1; // Swallow the key press
                }
            }
            
            // Pass other key events to the switcher window if it's visible
            if (g_switcher && g_switcher->IsVisible()) {
                // Use consistent shift detection method
                bool shiftPressed = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000);
                
                if (pkbhs->vkCode == VK_TAB) {
                    PostMessage(g_switcher->GetHwnd(), WM_APP_KEYDOWN, pkbhs->vkCode, MAKELPARAM(pkbhs->scanCode, shiftPressed ? 1 : 0));
                    return 1; // Swallow the key press
                }

                // Forward other key presses to the TabSwitcher window
                LPARAM packedLParam = MAKELPARAM(pkbhs->scanCode, shiftPressed ? 1 : 0);
                PostMessage(g_switcher->GetHwnd(), WM_APP_KEYDOWN, pkbhs->vkCode, packedLParam);
                return 1; // Swallow the key press
            }
        }
    }
    
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Config::LoadConfig(); // Load all settings from config.ini

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    g_hotkey = Utils::LoadHotkeySetting();

    // Use a mutex to ensure only one instance of the application runs
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"TabSwitcherMutex");
    if (hMutex == NULL) {
        return 1; // Mutex creation failed
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Find and close the existing window/process
        HWND existingHwnd = FindWindowW(L"TabSwitcherWindowClass", L"Tab Switcher");
        if (existingHwnd) {
            PostMessage(existingHwnd, WM_CLOSE, 0, 0);
        }
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        // Give the old process a moment to exit before the new one starts
        Sleep(100); 
    }

    CoInitialize(nullptr);

    g_switcher = std::make_unique<TabSwitcher>();
    if (!g_switcher->Create()) {
        MessageBoxW(nullptr, L"Failed to create TabSwitcher window", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!g_keyboardHook) {
        MessageBoxW(nullptr, L"Failed to set keyboard hook", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
    }
    
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    // g_switcher unique_ptr will handle cleanup
    CoUninitialize();
    return 0;
} 