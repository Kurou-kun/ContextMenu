#pragma once
#include <windows.h>
#include <string>

// One MenuHost per skin window. Installs a single window subclass that intercepts
// right-clicks and shows our custom menu, suppressing Rainmeter's default menu.
class MenuHost {
public:
    // Attach a subclass to hwnd (once per window). Returns the host for this window;
    // a second measure on the same window shares it and becomes the active source.
    static MenuHost* Attach(HWND hwnd, void* skin, void* rm, const std::wstring& menuPath);
    // Point the window's menu at this measure's file (last writer wins).
    void SetActive(void* skin, void* rm, const std::wstring& menuPath);
    // Remove one measure's claim; unsubclass + delete when the last measure leaves.
    static void Detach(HWND hwnd);

private:
    HWND         hwnd_ = nullptr;
    void*        skin_ = nullptr;
    void*        rm_   = nullptr;
    std::wstring menuPath_;
    int          refs_ = 0;

    void ShowMenu(POINT screenPt);
    static LRESULT CALLBACK SubProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};
