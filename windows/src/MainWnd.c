#include "main.h"

#include <stdio.h>
#include <tchar.h>
#include "resource.h"

struct MainWndCtx {
    HWND sheetCtrl;
    TCHAR filePath[4096];
};

static const LPCTSTR WndClass = _T("SCMainWnd");

static LRESULT CALLBACK OnFileNew(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct MainWndCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);
    SC_CHECK(ctx, "Failed to get window context");

    ctx->filePath[0] = 0;
    SetWindowText(wnd, SC_TITLE);

    return 0;
}

static LRESULT CALLBACK OnFileOpen(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct MainWndCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);
    SC_CHECK(ctx, "Failed to get window context");

    TCHAR path[4096];
    _sntprintf(path, _countof(path), "%s", ctx->filePath);
    
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = wnd;
    ofn.lpstrFilter =
        _T("Spreadsheets (*.sc, *.xlsx, *.xls, *.csv, *.tsv, *.txt, *.tab)\0")
        _T("*.sc;*.xlsx;*.xls;*.csv;*.tsv;*.txt;*.tab\0")
        _T("All files (*.*)\0*.*\0");
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES;

    if (!GetOpenFileName(&ofn))
        return 0;

    _sntprintf(ctx->filePath, _countof(ctx->filePath), "%s", ctx->filePath);

    TCHAR title[1024];
    _sntprintf(title, _countof(title),
        _T("%s - ") SC_TITLE, path + ofn.nFileOffset);
    SetWindowText(wnd, title);
    
    return 0;
}

static LRESULT CALLBACK OnFileClose(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    DestroyWindow(wnd);
    return 0;
}

static LRESULT CALLBACK OnSize(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct MainWndCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);

    if (ctx) {
        MoveWindow(ctx->sheetCtrl, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}

static LRESULT CALLBACK OnDestroy(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct MainWndCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);
    
    if (ctx) {
        SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)NULL);
    }

    PostQuitMessage(0);

    return DefWindowProc(wnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    WNDPROC handler = DefWindowProc;

    switch (msg) {
    case WM_COMMAND:
        switch LOWORD(wParam) {
        case ID_FILE_NEW: handler = OnFileNew; break;
        case ID_FILE_OPEN: handler = OnFileOpen; break;
        case ID_FILE_CLOSE: handler = OnFileClose; break;
        
        default: break;
        }
        break;

    case WM_SIZE: handler = OnSize; break;
    case WM_DESTROY: handler = OnDestroy; break;

    default: break;
    }

    return handler(wnd, msg, wParam, lParam);
}

HWND SCCreateMainWnd(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(wc);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = WndClass;
        wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN_MENU);
        wc.lpfnWndProc = WndProc;
        wc.cbWndExtra = sizeof(LONG_PTR);

        SC_CHECK(
            RegisterClassEx(&wc),
            _T("Failed to register window class"));

        registered = TRUE;
    }

    HWND wnd = CreateWindow(
        WndClass,
        SC_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    SC_CHECK(wnd, _T("Failed to create window"));

    RECT clientRect = {0, 0, 100, 100};
    GetClientRect(wnd, &clientRect);

    struct MainWndCtx *ctx = calloc(1, sizeof(struct MainWndCtx));
    ctx->sheetCtrl = SCCreateSheetCtrl(hInstance, wnd, clientRect);

    SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)ctx);
    SC_CHECK(!GetLastError(), _T("Failed to set window context"));

    return wnd;
}
