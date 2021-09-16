#include "systray.h"

#include "../defer.hpp"

#include <spdlog/spdlog.h>

#include <mutex>
#include <thread>
#include <atomic>

#define APPWM_ICONNOTIFY (WM_APP + 1)

#include <windows.h>
#include <shellapi.h>

namespace sc::systray {

    static std::atomic_bool worker_running = false;
    static std::mutex worker_mutex;
    static std::thread worker;

    static std::optional<std::function<void()>> interact_cb;

    static LRESULT CALLBACK windows_messaging_cb(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case APPWM_ICONNOTIFY: {
                switch (lParam) {
                    case WM_LBUTTONUP:
                    case WM_RBUTTONUP:
                        if (interact_cb.has_value() && interact_cb.value()) interact_cb.value()();
                        break;
                }
                return 0;
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void work_func() {
        spdlog::debug("System tray thread: {}", std::hash<std::thread::id>()(std::this_thread::get_id()));
        WNDCLASS dummy_window_class = { };
        dummy_window_class.lpfnWndProc = windows_messaging_cb;
        dummy_window_class.hInstance = GetModuleHandle(NULL);
        dummy_window_class.lpszClassName = "SonicSysTrayClass";
        if (RegisterClass(&dummy_window_class)) {
            if (auto dummy_window = CreateWindow(dummy_window_class.lpszClassName, "Dummy", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, dummy_window_class.hInstance, NULL); dummy_window) {
                spdlog::debug("Created dummy window for system tray: {}", reinterpret_cast<void *>(dummy_window));
                NOTIFYICONDATA notify_icon_data = { };
                notify_icon_data.cbSize = sizeof(NOTIFYICONDATA);
                notify_icon_data.uVersion = NOTIFYICON_VERSION_4;
                notify_icon_data.hWnd = dummy_window;
                notify_icon_data.uID = 1;
                notify_icon_data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
                notify_icon_data.uCallbackMessage = APPWM_ICONNOTIFY;
                notify_icon_data.hIcon = static_cast<HICON>(LoadImage(dummy_window_class.hInstance, "TRAY_ICON", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
                if (notify_icon_data.hIcon == NULL) spdlog::warn("Failed to create icon for system tray.");
                strcpy_s(notify_icon_data.szTip, "Sim Coaches");
                if (Shell_NotifyIcon(NIM_ADD, &notify_icon_data) == TRUE) {
                    while (worker_running) {
                        MSG msg;
                        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        std::this_thread::yield();
                    }
                    if (Shell_NotifyIcon(NIM_DELETE, &notify_icon_data) == FALSE) spdlog::warn("Failed to delete shell NIM for system tray.");
                } else spdlog::error("Failed to add shell NIM for system tray.");
                if (DestroyIcon(notify_icon_data.hIcon) == FALSE) spdlog::warn("Failed to destroy icon for system tray.");
                ShutdownBlockReasonDestroy(dummy_window);
                if (DestroyWindow(notify_icon_data.hWnd) == FALSE) spdlog::warn("Failed to destroy dummy window for system tray.");
            } else spdlog::error("Unable to create window for system tray.");
            if (UnregisterClass(dummy_window_class.lpszClassName, dummy_window_class.hInstance) == FALSE) spdlog::warn("Failed to unregister the window class for system tray.");
        } else spdlog::error("Unable to register class for system tray.");
        spdlog::debug("Exiting system tray thread now.");
    }
}

void sc::systray::enable(std::optional<std::function<void()>> interact_cb) {
    disable();
    spdlog::debug("Starting system tray module...");
    systray::interact_cb = interact_cb;
    worker_running = true;
    worker = std::thread(work_func);
}

void sc::systray::disable() {
    worker_running = false;
    if (worker.joinable()) {
        spdlog::debug("Stopping system tray module...");
        worker.join();
    }
    interact_cb.reset();
}