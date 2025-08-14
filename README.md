# TabSwitcher

A simple, rofi-like window switcher for Windows.

## Description

This application provides a fast, keyboard-driven way to switch between open windows. It is designed to be lightweight and minimalistic.

## Features

- Fuzzy search for open windows.
- Activation via `RSHIFT` hotkey.
- Navigation with arrow keys.
- Window activation with `Enter`.

## Build Instructions

To build the project, you need to have CMake and a C++ compiler (like MSVC from Visual Studio) installed.

1.  Open a terminal in the project's root directory.
2.  Configure the project with CMake:
    ```sh
    cmake -S . -B build
    ```
3.  Build the project:
    ```sh
    cmake --build build
    ```
4.  The executable will be located at `build/Debug/tabswitcher.exe`.

## Usage

1.  Run the executable `build/Debug/tabswitcher.exe`. It will run in the background.
2.  Press `RSHIFT` to show the window switcher.
3.  Start typing to filter the list of open windows.
4.  Use the `Up` and `Down` arrow keys to navigate the list.
5.  Press `Enter` to switch to the selected window.
6.  Press `Esc` to hide the switcher without changing the window.

## Window Event Hook

The switcher keeps its window list updated using a WinEvent hook. During startup
`StartWindowUpdater` registers a hook with `SetWinEventHook` for
`EVENT_OBJECT_CREATE`, `EVENT_OBJECT_DESTROY` and `EVENT_SYSTEM_FOREGROUND`.
Each event updates only the affected window entry.  When the application exits
`StopWindowUpdater` automatically unregisters the hook.  If hook registration
fails the switcher falls back to a slower polling mode.

## Changes Made

- **Fixed Keyboard Input:** Repaired the faulty keyboard input handling that prevented the search functionality from working correctly. The application now properly processes keystrokes for searching and navigation.
- **Changed Hotkey:** The hotkey to activate the switcher has been set to `RSHIFT`.
- **Improved Stability:** Refactored the way keyboard events are passed from the global hook to the application window, preventing crashes and undefined behavior related to invalid memory access.
- **WinEvent Hook:** Window list updates now rely on a `SetWinEventHook` that tracks
  window creation, destruction and foreground changes. The hook is registered at
  startup and unhooked automatically on shutdown, with a polling fallback if the
  registration fails.