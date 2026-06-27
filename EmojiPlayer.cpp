#include <windows.h>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>

// ----------------------------------------------------------------
//  SEQUENTIAL EMOJI PLAYER WITH ZOOM & PAN
//  - Displays one emoji at a time, cycling through a predefined list
//  - Counter shows the index (1,2,3,...)
//  Controls:
//    Space  - pause/resume
//    + / -  - zoom in/out
//    L      - pan left
//    R      - pan right
// ----------------------------------------------------------------

// --- Emoji list (Unicode wide strings) ---
std::vector<std::wstring> emojis = {
    L"😀", L"😁", L"😂", L"🤣", L"😃", L"😄", L"😅", L"😆", L"😉", L"😊",
    L"😋", L"😎", L"😍", L"🥰", L"😘", L"😗", L"😙", L"😚", L"☺️", L"🙂",
    L"🤗", L"🤩", L"🤔", L"🤨", L"😐", L"😑", L"😶", L"🙄", L"😏", L"😣",
    L"😥", L"😮", L"🤐", L"😯", L"😪", L"😫", L"😴", L"😌", L"😛", L"😜",
    L"😝", L"🤤", L"😒", L"😓", L"😔", L"😕", L"🙃", L"🤑", L"😲", L"☹️",
    L"🙁", L"😖", L"😞", L"😟", L"😤", L"😢", L"😭", L"😦", L"😧", L"😨",
    L"😩", L"🤯", L"😬", L"😰", L"😱", L"🥵", L"🥶", L"😳", L"🤪", L"😵",
    L"🥴", L"😠", L"😡", L"🤬", L"😷", L"🤒", L"🤕", L"🤢", L"🤮", L"🥺",
    L"😈", L"👿", L"👹", L"👺", L"💀", L"☠️", L"👻", L"👽", L"👾", L"🤖",
    L"🎃", L"😺", L"😸", L"😹", L"😻", L"😼", L"😽", L"🙀", L"😿", L"😾"
};

// --- Global state ---
HWND g_hWnd = nullptr;
bool g_paused = false;
int g_index = 0;           // current emoji index (0-based)
int g_zoom = 100;          // 100% = normal
int g_panX = 0;
const int PAN_STEP = 2;

// Offscreen buffer
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;
int g_winWidth = 0, g_winHeight = 0;

// --- Rendering ---
void RenderScene(HDC hdc) {
    // Clear to dark background
    RECT rect = {0, 0, g_winWidth, g_winHeight};
    HBRUSH bg = CreateSolidBrush(RGB(10,10,30));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);

    // Determine font size based on zoom
    int fontSize = 120 * g_zoom / 100;   // base 120pt
    if (fontSize < 20) fontSize = 20;
    if (fontSize > 500) fontSize = 500;

    LOGFONT lf = {};
    lf.lfHeight = -fontSize;
    // Use a font that supports emoji (Segoe UI Emoji is good on Windows)
    wcscpy_s(lf.lfFaceName, L"Segoe UI Emoji");
    lf.lfWeight = FW_REGULAR;
    HFONT hFont = CreateFontIndirect(&lf);

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    // Get current emoji string
    std::wstring emoji = emojis[g_index % emojis.size()];

    // Measure text width for centering
    SIZE size;
    GetTextExtentPoint32W(hdc, emoji.c_str(), emoji.length(), &size);
    int xPos = (g_winWidth - size.cx) / 2 + g_panX;
    int yPos = (g_winHeight - size.cy) / 2;

    // Draw emoji in bright color (white)
    SetTextColor(hdc, RGB(255,255,255));
    SetBkColor(hdc, RGB(10,10,30));
    TextOutW(hdc, xPos, yPos, emoji.c_str(), emoji.length());

    // Draw status info (top left)
    SetTextColor(hdc, RGB(200,200,200));
    SetBkColor(hdc, RGB(10,10,30));
    wchar_t info[256];
    swprintf_s(info, L"Index: %d / %d  |  Zoom: %d%%  |  Pan: %d  |  %s",
               g_index + 1, (int)emojis.size(), g_zoom, g_panX, g_paused ? L"PAUSED" : L"PLAYING");
    TextOutW(hdc, 10, 10, info, wcslen(info));

    // Draw emoji name (optional) at bottom
    std::wstring emojiName = L"Emoji: " + emoji;
    TextOutW(hdc, 10, g_winHeight - 30, emojiName.c_str(), emojiName.length());

    // Clean up
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 33, NULL); // 30 FPS
            return 0;
        }
        case WM_SIZE: {
            g_winWidth = LOWORD(lParam);
            g_winHeight = HIWORD(lParam);
            if (g_hbmMem) DeleteObject(g_hbmMem);
            HDC hdc = GetDC(hwnd);
            g_hbmMem = CreateCompatibleBitmap(hdc, g_winWidth, g_winHeight);
            ReleaseDC(hwnd, hdc);
            return 0;
        }
        case WM_TIMER: {
            if (!g_paused) {
                g_index++; // next emoji in sequence
                if (g_index >= emojis.size()) g_index = 0; // loop
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_hbmMem) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, g_hbmMem);
                RenderScene(hdcMem);
                BitBlt(hdc, 0, 0, g_winWidth, g_winHeight, hdcMem, 0, 0, SRCCOPY);
                DeleteDC(hdcMem);
            } else {
                RenderScene(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_SPACE:
                    g_paused = !g_paused;
                    SetWindowTextW(hwnd, g_paused ? L"Emoji Player [PAUSED]" : L"Emoji Player [PLAYING]");
                    break;
                case VK_OEM_PLUS:
                case VK_ADD:
                    g_zoom += 10;
                    if (g_zoom > 300) g_zoom = 300;
                    break;
                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                    g_zoom -= 10;
                    if (g_zoom < 20) g_zoom = 20;
                    break;
                case 'L':
                case 'l':
                    g_panX += PAN_STEP;
                    break;
                case 'R':
                case 'r':
                    g_panX -= PAN_STEP;
                    break;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            if (g_hbmMem) DeleteObject(g_hbmMem);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EmojiPlayerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BLACK+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    g_winWidth = 800;
    g_winHeight = 600;
    g_hWnd = CreateWindowExW(
        0, L"EmojiPlayerClass", L"Emoji Player [PLAYING]",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, g_winWidth, g_winHeight,
        NULL, NULL, hInstance, NULL
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
