#include <windows.h>
#include <objbase.h>
#include <wmp.h>
#include <commdlg.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

// --- Global handles ---
HWND g_hWnd = nullptr;
IWMPPlayer* g_pPlayer = nullptr;

// --- Forward declarations ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void OpenFile();

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Init COM
    CoInitialize(nullptr);

    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"VideoPlayerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BLACK + 1);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszMenuName  = nullptr;
    RegisterClass(&wc);

    // Create window
    g_hWnd = CreateWindowEx(
        0, L"VideoPlayerClass", L"Video Player (like VLC)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hWnd) {
        CoUninitialize();
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (g_pPlayer) {
        g_pPlayer->Release();
        g_pPlayer = nullptr;
    }
    CoUninitialize();
    return 0;
}

// --- Create WMP control ---
void CreatePlayer(HWND hwnd) {
    HRESULT hr = CoCreateInstance(
        CLSID_WindowsMediaPlayer,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWMPPlayer,
        (void**)&g_pPlayer
    );
    if (FAILED(hr)) {
        MessageBox(hwnd, L"Failed to create WMP control", L"Error", MB_OK);
        return;
    }

    // Attach to the window (display area is entire client area)
    g_pPlayer->put_windowHandle((LONG_PTR)hwnd);

    // Show full transport controls (play/pause, seek, volume, etc.)
    g_pPlayer->put_uiMode(L"full");

    // Optionally set a default media (none)
}

// --- Open file dialog and load video ---
void OpenFile() {
    if (!g_pPlayer) return;

    OPENFILENAME ofn = {};
    wchar_t filePath[MAX_PATH] = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = g_hWnd;
    ofn.lpstrFile       = filePath;
    ofn.nMaxFile        = MAX_PATH;
    ofn.lpstrFilter     = L"Video Files\0*.mp4;*.avi;*.wmv;*.mkv;*.mov;*.flv;*.webm\0All Files\0*.*\0";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        // Build file:// URL
        std::wstring url = L"file:///" + std::wstring(filePath);
        // Replace backslashes with forward slashes
        for (auto& c : url) if (c == L'\\') c = L'/';

        g_pPlayer->put_URL(url.c_str());
        SetWindowText(g_hWnd, (L"Playing: " + std::wstring(filePath)).c_str());
    }
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreatePlayer(hwnd);
            // Create a simple "Open" button (or just use menu)
            HMENU hMenu = CreateMenu();
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, 1, L"&Open Video");
            AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenu(hFileMenu, MF_STRING, 2, L"E&xit");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
            SetMenu(hwnd, hMenu);
            return 0;
        }

        case WM_SIZE: {
            // Resize the video display automatically via WMP
            if (g_pPlayer) {
                g_pPlayer->put_windowHandle((LONG_PTR)hwnd);
            }
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1: OpenFile(); break;
                case 2: DestroyWindow(hwnd); break;
            }
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            wchar_t filePath[MAX_PATH];
            DragQueryFile(hDrop, 0, filePath, MAX_PATH);
            DragFinish(hDrop);
            if (g_pPlayer) {
                std::wstring url = L"file:///" + std::wstring(filePath);
                for (auto& c : url) if (c == L'\\') c = L'/';
                g_pPlayer->put_URL(url.c_str());
                SetWindowText(hwnd, (L"Playing: " + std::wstring(filePath)).c_str());
            }
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
