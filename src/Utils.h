#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <regex>

#include "Config.h" // Include the centralized config file

// Window information structure
struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    std::wstring className;
    std::wstring processName;
    DWORD processId;
    bool isVisible;
    bool isMinimized;
    HICON icon;
    bool destroyIcon;
    double score = 0.0;

    WindowInfo()
        : hwnd(nullptr), processId(0), isVisible(false),
          isMinimized(false), icon(nullptr), destroyIcon(false),
          score(0.0) {}

    ~WindowInfo() {
        if (destroyIcon && icon) {
            DestroyIcon(icon);
        }
    }

    WindowInfo(const WindowInfo& other)
        : hwnd(other.hwnd), title(other.title), className(other.className),
          processName(other.processName), processId(other.processId),
          isVisible(other.isVisible), isMinimized(other.isMinimized),
          icon(nullptr), destroyIcon(other.destroyIcon),
          score(other.score) {
        if (other.icon) {
            icon = destroyIcon ? CopyIcon(other.icon) : other.icon;
        }
    }

    WindowInfo& operator=(const WindowInfo& other) {
        if (this != &other) {
            if (destroyIcon && icon) {
                DestroyIcon(icon);
            }

            hwnd = other.hwnd;
            title = other.title;
            className = other.className;
            processName = other.processName;
            processId = other.processId;
            isVisible = other.isVisible;
            isMinimized = other.isMinimized;
            score = other.score;
            destroyIcon = other.destroyIcon;

            if (other.icon) {
                icon = destroyIcon ? CopyIcon(other.icon) : other.icon;
            } else {
                icon = nullptr;
            }
        }
        return *this;
    }

    WindowInfo(WindowInfo&& other) noexcept
        : hwnd(other.hwnd), title(std::move(other.title)),
          className(std::move(other.className)),
          processName(std::move(other.processName)),
          processId(other.processId), isVisible(other.isVisible),
          isMinimized(other.isMinimized), icon(other.icon),
          destroyIcon(other.destroyIcon), score(other.score) {
        other.icon = nullptr;
        other.destroyIcon = false;
    }

    WindowInfo& operator=(WindowInfo&& other) noexcept {
        if (this != &other) {
            if (destroyIcon && icon) {
                DestroyIcon(icon);
            }

            hwnd = other.hwnd;
            title = std::move(other.title);
            className = std::move(other.className);
            processName = std::move(other.processName);
            processId = other.processId;
            isVisible = other.isVisible;
            isMinimized = other.isMinimized;
            icon = other.icon;
            destroyIcon = other.destroyIcon;
            score = other.score;

            other.icon = nullptr;
            other.destroyIcon = false;
        }
        return *this;
    }
};

// Utility functions
namespace Utils {
    std::wstring GetProcessName(DWORD processId);
    HICON GetWindowIcon(HWND hwnd, bool& destroyIcon);
    void CenterWindow(HWND hwnd, int width, int height);
    bool IsValidWindow(HWND hwnd);
    UINT StringToVK(const std::string& key);
    UINT LoadHotkeySetting();
}