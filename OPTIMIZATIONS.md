# TabSwitcher Optimizations and Improvements

This document outlines potential optimizations, bug fixes, and general improvements for the TabSwitcher application.

## 1. Performance Optimizations

### 1.1. Window Enumeration (`GetAllWindows`)
- **Issue:** `GetAllWindows()` is called every time the switcher is shown. This function enumerates all top-level windows on the system, which can be resource-intensive if many applications are running.
- **Suggestion:** While showing an up-to-date list is crucial, the process can be optimized. Instead of a full re-scan, consider using a `WH_SHELL` hook to receive notifications for window creation (`HSHELL_WINDOWCREATED`) and destruction (`HSHELL_WINDOWDESTROYED`). This allows for maintaining a cached list of windows that is updated incrementally, which is significantly more performant.

### 1.2. Process Name Resolution (`GetProcessName`)
- **Issue:** The `GetProcessName` function is called for every single window during the enumeration process. It works by taking a snapshot of all running processes and iterating through them to find a matching process ID. This is highly inefficient and likely a major performance bottleneck.
- **Suggestion:** Optimize this by creating a map of `process ID -> process name` once at the beginning of the `GetAllWindows` call. Then, for each window, look up the process name in this map instead of re-scanning all system processes every time.

### 1.3. GDI Object Management
- **Issue:** GDI objects like `HBRUSH` and `HPEN` are created and destroyed within the `DrawSearchBox` and `DrawWindowItem` functions, which are called on every `WM_PAINT` message. This is inefficient and can lead to performance degradation, especially during rapid UI updates (e.g., scrolling).
- **Suggestion:** Create all necessary GDI resources (brushes, pens) once during the `TabSwitcher::Create` method and store them as class members. They can then be reused in the paint handlers and should be cleaned up in the `TabSwitcher` destructor.

### 1.4. Redundant Full Redraws
- **Issue:** `InvalidateRect(m_hwnd, nullptr, TRUE)` is called frequently, which forces the entire window to be repainted.
- **Suggestion:** Use more targeted invalidation. For example, when the selection changes, only invalidate the rectangles of the previously selected item and the newly selected item. This is already correctly implemented for the caret timer but can be applied more broadly.

### 1.5. Regex Compilation
- **Issue:** `std::wregex` is compiled on every keypress in the search box.
- **Suggestion:** While the pattern changes on each keypress, `std::wregex` can be slow. For simple substring matching, the existing fallback implementation (using `std::wstring::find`) is much faster. Consider making this the default search method and only enabling regex matching if the user enters a specific prefix or syntax. This would improve search responsiveness.

## 2. Bug Fixes and General Improvements

### 2.1. Single-Instance Logic (Mutex)
- **Issue:** The current implementation finds and closes the existing application instance, which is aggressive and can lead to data loss. It also uses a `Sleep(100)` call, which is not a reliable way to ensure the old process has exited before the new one continues.
- **Suggestion:** Refactor the logic. If the mutex already exists, the new instance should not try to kill the old one. Instead, it should find the existing window handle, bring it to the foreground (`SetForegroundWindow`), and then exit immediately.

### 2.2. GDI Resource Leak
- **Issue:** The `HBRUSH` created for the window class background (`wc.hbrBackground`) in `RegisterWindowClass` is never deleted, resulting in a resource leak.
- **Suggestion:** Store the handle to this brush as a class member in `TabSwitcher` and explicitly delete it in the destructor.

### 2.3. Keyboard Input Handling
- **Issue:** The keyboard input handling is overly complex. A low-level hook posts a custom `WM_APP_KEYDOWN` message, and the handler (`OnCustomKeyDown`) then attempts to re-translate virtual key codes back into characters. This is fragile and error-prone.
- **Suggestion:** Simplify the input pipeline. The hook should post standard messages like `WM_KEYDOWN`, `WM_SYSKEYDOWN`, and `WM_CHAR` to the switcher window. This allows the window procedure to handle input in a standard, predictable way, removing the need for `OnCustomKeyDown` and its complex logic.

### 2.4. Deprecated API Usage
- **Issue:** `keybd_event` is used in `WindowManager::ActivateWindow`. This function has been superseded by `SendInput`.
- **Suggestion:** Replace the call to `keybd_event` with the more modern and reliable `SendInput` API.

### 2.5. Window Filtering (`IsValidWindow`)
- **Issue:** The rules for which windows to show are hardcoded and contain commented-out logic, making it unclear what the intended behavior is. The current filtering might exclude useful windows or include irrelevant ones.
- **Suggestion:** Solidify the filtering logic. Add comments to explain why certain styles or class names are excluded. Consider making the filtering rules configurable (e.g., via a settings file) to allow users to include or exclude certain applications.

### 2.6. Hardcoded Hotkey
- **Issue:** The activation hotkey is hardcoded to `RSHIFT`.
- **Suggestion:** Make the hotkey configurable. This could be done through a simple configuration file (`config.ini`) or registry entry, providing more flexibility for the user.

### 2.7. Error Handling
- **Issue:** Error handling for Win32 API calls is inconsistent. For example, the `CreateMutexW` call doesn't handle failures other than `ERROR_ALREADY_EXISTS`.
- **Suggestion:** Add robust error checking for all critical API calls and provide informative error messages to the user (e.g., via `MessageBoxW`) when something goes wrong.

### 2.8. Magic Strings
- **Issue:** The mutex name `"TabSwitcherMutex"` is a magic string in `main.cpp`.
- **Suggestion:** Define this and other similar constants in a centralized place, like `Config.h`, to improve maintainability and avoid potential typos.
