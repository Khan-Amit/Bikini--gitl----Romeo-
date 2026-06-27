#include <windows.h>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

// ----------------------------------------------------------------
//  SIMPLE SEQUENCE PLAYER WITH ZOOM & PAN
//  Controls:
//    Space  - pause/resume animation
//    + / -  - zoom in/out
//    L      - pan left (step 1~3, adjustable)
//    R      - pan right (step 1~3)
// ----------------------------------------------------------------

// --- Global state ---
HWND g_hWnd = nullptr;
bool g_paused = false;
int g_counter = 1;
int g_zoom = 100;          // 100% = normal
int g_panX = 0;            // pixels offset
const int PAN_STEP = 2;    // 1~3 as requested, we use 2

// Offscreen buffer
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;
int g_winWidth = 0, g_winHeight = 0;

// --- Rendering ---
void RenderScene(HDC hdc) {
    // Clear to dark background
    RECT rect = {0, 0, g_winWidth, g_winHeight};
    HBRUSH bg = CreateSolidBrush(RGB(10,10,20));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);

    // Set up transformation: zoom and pan
    // We'll just scale and offset the text position
    int centerX = g_winWidth / 2;
    int centerY = g_winHeight / 2;

    // Calculate font size based on zoom (base 72pt, zoom factor)
    int fontSize = 72 * g_zoom / 100;
    if (fontSize < 10) fontSize = 10;
    if (fontSize > 300) fontSize = 300;

    // Create font
    LOGFONT lf = {};
    lf.lfHeight = -fontSize; // negative for height
    wcscpy_s(lf.lfFaceName, L"Courier New");
    lf.lfWeight = FW_BOLD;
    HFONT hFont = CreateFontIndirect(&lf);

    // Select font
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    // Build the number string (show 24^n if you want, but we just show counter)
    std::wstringstream wss;
    wss << g_counter;
    std::wstring text = wss.str();

    // Apply pan (horizontal shift)
    int xPos = centerX - (text.length() * fontSize / 2) + g_panX;
    int yPos = centerY - fontSize / 2;

    // Draw with bright green color
    SetTextColor(hdc, RGB(0, 255, 200));
    SetBkColor(hdc, RGB(10,10,20));
    TextOutW(hdc, xPos, yPos, text.c_str(), text.length());

    // Draw status info (top left)
    SetTextColor(hdc, RGB(200,200,200));
    SetBkColor(hdc, RGB(10,10,20));
    wchar_t info[256];
    swprintf_s(info, L"Counter: %d  |  Zoom: %d%%  |  Pan: %d  |  %s",
               g_counter, g_zoom, g_panX, g_paused ? L"PAUSED" : L"PLAYING");
    TextOutW(hdc, 10, 10, info, wcslen(info));

    // Clean up font
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
            // Recreate offscreen buffer
            if (g_hbmMem) DeleteObject(g_hbmMem);
            HDC hdc = GetDC(hwnd);
            g_hbmMem = CreateCompatibleBitmap(hdc, g_winWidth, g_winHeight);
            ReleaseDC(hwnd, hdc);
            return 0;
        }
        case WM_TIMER: {
            if (!g_paused) {
                // Increment counter (simulate sequence: 1,2,3,... up to 24^n)
                g_counter++;
                if (g_counter > 1000000) g_counter = 1; // avoid overflow
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Render to offscreen
            if (g_hbmMem) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, g_hbmMem);
                RenderScene(hdcMem);
                BitBlt(hdc, 0, 0, g_winWidth, g_winHeight, hdcMem, 0, 0, SRCCOPY);
                DeleteDC(hdcMem);
            } else {
                // fallback: render directly
                RenderScene(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_SPACE:
                    g_paused = !g_paused;
                    SetWindowTextW(hwnd, g_paused ? L"Sequence Player [PAUSED]" : L"Sequence Player [PLAYING]");
                    break;
                case VK_OEM_PLUS:   // '+'
                case VK_ADD:        // numpad '+'
                    g_zoom += 10;
                    if (g_zoom > 300) g_zoom = 300;
                    break;
                case VK_OEM_MINUS:  // '-'
                case VK_SUBTRACT:   // numpad '-'
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
    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SequencePlayerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BLACK+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create window
    g_winWidth = 800;
    g_winHeight = 600;
    g_hWnd = CreateWindowExW(
        0, L"SequencePlayerClass", L"Sequence Player [PLAYING]",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, g_winWidth, g_winHeight,
        NULL, NULL, hInstance, NULL
    );

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
