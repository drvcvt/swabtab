#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <iostream>

struct WindowInfo {
    std::wstring title;
    std::wstring titleLower;
    std::wstring processName;
    std::wstring processLower;
};

std::vector<WindowInfo> Filter(const std::vector<WindowInfo>& windows, const std::wstring& search) {
    std::vector<WindowInfo> filtered;
    if (search.empty()) {
        return windows;
    }

    std::wstring search_lower = search;
    std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::towlower);

    bool useFuzzy = search_lower.size() <= 2;
    auto score_fn = [&](const std::wstring& target) -> double {
        if (target.rfind(search_lower, 0) == 0)
            return 100.0;
        if (target.find(search_lower) != std::wstring::npos)
            return 80.0;
        if (useFuzzy) // simplified: no fuzzy search implementation needed for this test
            return 0.0;
        return 0.0;
    };

    for (const auto& w : windows) {
        double title_score = score_fn(w.titleLower);
        double process_score = score_fn(w.processLower);
        double final_score = std::max(title_score, process_score);
        if (final_score > 50)
            filtered.push_back(w);
    }
    return filtered;
}

int main() {
    WindowInfo info;
    info.title = L"Some Title";
    info.titleLower = L"some title";
    info.processName = L"Other.EXE";
    info.processLower = L"other";

    std::vector<WindowInfo> windows{info};

    // Title matches regardless of case
    assert(Filter(windows, L"some").size() == 1);
    assert(Filter(windows, L"SOME").size() == 1);

    // Process name matches regardless of case
    assert(Filter(windows, L"other").size() == 1);
    assert(Filter(windows, L"OTHER").size() == 1);

    // Non-matching query
    assert(Filter(windows, L"missing").empty());

    std::wcout << L"All tests passed\n";
    return 0;
}
