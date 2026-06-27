#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <filesystem>
#include <shlobj.h>     // for folder dialog

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;
namespace fs = std::filesystem;

// ----------------------------------------------------------------
//  SEQUENTIAL IMAGE PLAYER
//  - Loads all images from a selected folder
//  - Displays them one by one (like a slideshow)
//  - Controls: Space=pause, +/-=zoom, L/R=pan
// ----------------------------------------------------------------

// --- Global state ---
HWND g_hWnd = nullptr;
bool g_paused = false;
int g_currentIndex = 0;
int g_zoom = 100;              // percentage
int g_panX = 0;
const int PAN_STEP = 2;
std::vector<std::wstring> g_imageFiles;   // full paths
std::vector<Image*> g_images;             // loaded GDI+ images (optional caching)
int g_winWidth = 800, g_winHeight = 600;
bool g_loaded = false;

// Offscreen buffer
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;

// Forward declarations
bool LoadImagesFromFolder(const std::wstring& folderPath);
void RenderScene(HDC hdc);

// --- Folder selection dialog ---
std::wstring SelectFolder() {
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"Select folder containing your images";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != nullptr) {
        wchar_t path[MAX_PATH];
        SHGetPathFromIDListW(pidl, path);
        CoTaskMemFree(pidl);
        return std::wstring(path);
    }
    return L"";
}

// --- Load images from folder (filter common extensions) ---
bool LoadImagesFromFolder(const std::wstring& folderPath) {
    g_imageFiles.clear();
    // Clear old cached images
    for (auto img : g_images) {
        delete img;
    }
    g_images.clear();

    try {
        for (const auto& entry : fs::directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::wstring ext = entry.path().extension().wstring();
                // Convert to lower
                for (auto& c : ext) c = towlower(c);
                if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" ||
                    ext == L".bmp" || ext == L".gif" || ext == L".tiff" || ext == L".tif") {
                    g_imageFiles.push_back(entry.path().wstring());
                }
            }
        }
    } catch (...) {
        return false;
    }

    if (g_imageFiles.empty()) return false;

    // Sort alphabetically for consistent order
    std::sort(g_imageFiles.begin(), g_imageFiles.end());

    // Optionally preload images (for speed) – but we'll load on-demand to save memory
    // We'll load each image when it's time to display it
    g_currentIndex = 0;
    g_loaded = true;
    return true;
}

// --- Load a specific image (returns Gdiplus::Image*) ---
Image* LoadImageAtIndex(int index) {
    if (index < 0 || index >= (int)g_imageFiles.size()) return nullptr;
    return new Image(g_imageFiles[index].c_str());
}

// --- Rendering ---
void RenderScene(HDC hdc) {
    // Clear background
    RECT rect = {0, 0, g_winWidth, g_winHeight};
    HBRUSH bg = CreateSolidBrush(RGB(20,20,20));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);

    if (!g_loaded || g_imageFiles.empty()) {
        // Show message to select folder
        SetTextColor(hdc, RGB(255,255,255));
        SetBkColor(hdc, RGB(20,20,20));
        HFONT font = CreateFontW(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        SelectObject(hdc, font);
        const wchar_t* msg = L"Press 'O' to select image folder";
        SIZE size;
        GetTextExtentPoint32W(hdc, msg, wcslen(msg), &size);
        TextOutW(hdc, (g_winWidth - size.cx)/2, (g_winHeight - size.cy)/2, msg, wcslen(msg));
        DeleteObject(font);
        return;
    }

    // Get current image
    int idx = g_currentIndex % g_imageFiles.size();
    Image* img = LoadImageAtIndex(idx);
    if (!img) {
        // Show error
        SetTextColor(hdc, RGB(255,100,100));
        SetBkColor(hdc, RGB(20,20,20));
        TextOutW(hdc, 10, 10, L"Failed to load image", 21);
        return;
    }

    // Apply zoom and pan
    int imgWidth = img->GetWidth();
    int imgHeight = img->GetHeight();
    float scale = g_zoom / 100.0f;
    int drawWidth = (int)(imgWidth * scale);
    int drawHeight = (int)(imgHeight * scale);
    int x = (g_winWidth - drawWidth) / 2 + g_panX;
    int y = (g_winHeight - drawHeight) / 2;

    // Draw image
    Graphics graphics(hdc);
    graphics.DrawImage(img, x, y, drawWidth, drawHeight);

    // Draw info overlay
    SetTextColor(hdc, RGB(200,200,200));
    SetBkColor(hdc, RGB(20,20,20));
    wchar_t info[512];
    swprintf_s(info, L"Image %d/%d  |  Zoom: %d%%  |  Pan: %d  |  %s",
               idx+1, (int)g_imageFiles.size(), g_zoom, g_panX,
               g_paused ? L"PAUSED" : L"PLAYING");
    TextOutW(hdc, 10, 10, info, wcslen(info));

    // Show file name at bottom
    std::wstring fname = fs::path(g_imageFiles[idx]).filename().wstring();
    TextOutW(hdc, 10, g_winHeight - 30, fname.c_str(), fname.length());

    delete img;
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 33, NULL); // 30 FPS
            // Ask for folder on start
            std::wstring folder = SelectFolder();
            if (!folder.empty()) {
                LoadImagesFromFolder(folder);
            }
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
            if (!g_paused && g_loaded && !g_imageFiles.empty()) {
                g_currentIndex++;
                if (g_currentIndex >= (int)g_imageFiles.size()) g_currentIndex = 0;
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
                    SetWindowTextW(hwnd, g_paused ? L"Image Player [PAUSED]" : L"Image Player [PLAYING]");
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
                case 'O':
                case 'o': {
                    // Open folder selection again
                    std::wstring folder = SelectFolder();
                    if (!folder.empty()) {
                        LoadImagesFromFolder(folder);
                        g_currentIndex = 0;
                    }
                    break;
                }
                case VK_LEFT:   // manually go to previous
                    if (g_loaded && !g_imageFiles.empty()) {
                        g_currentIndex--;
                        if (g_currentIndex < 0) g_currentIndex = g_imageFiles.size() - 1;
                    }
                    break;
                case VK_RIGHT:  // manually go to next
                    if (g_loaded && !g_imageFiles.empty()) {
                        g_currentIndex++;
                        if (g_currentIndex >= (int)g_imageFiles.size()) g_currentIndex = 0;
                    }
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
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ImagePlayerClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BLACK+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create window
    g_winWidth = 800;
    g_winHeight = 600;
    g_hWnd = CreateWindowExW(
        0, L"ImagePlayerClass", L"Image Player [PLAYING]",
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

    GdiplusShutdown(gdiplusToken);
    return msg.wParam;
}
