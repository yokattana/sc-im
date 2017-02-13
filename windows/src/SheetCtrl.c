#include "main.h"
#include "compat.h"

#define CELL_WIDTH 65
#define CELL_HEIGHT 20

#define GRID_BGCOLOR RGB(0xFF, 0xFF, 0xFF)
#define GRID_LNCOLOR RGB(0xDD, 0xDD, 0xDD)

static const LPCTSTR WndClass = _T("SCSheetCtrl");

struct SheetCtrlCtx {
    HBRUSH gridBgBrush;
    HBRUSH gridLnBrush;
};

static LRESULT CALLBACK OnPaint(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct SheetCtrlCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);
    SC_CHECK(ctx, _T("Failed to get sheet control context"));

    if (!ctx->gridBgBrush) ctx->gridBgBrush = CreateSolidBrush(GRID_BGCOLOR);
    if (!ctx->gridLnBrush) ctx->gridLnBrush = CreateSolidBrush(GRID_LNCOLOR);

    PAINTSTRUCT paint;
    HDC hdc = BeginPaint(wnd, &paint);
    if (!hdc)
        return 0;

    UINT dpi = GetDpiForWindow(wnd);
    int cellWidth = SC_PX(CELL_WIDTH, dpi);
    int cellHeight = SC_PX(CELL_HEIGHT, dpi);

    FillRect(hdc, &paint.rcPaint, ctx->gridBgBrush);

    int startX = paint.rcPaint.left-1 + (-paint.rcPaint.left) % cellWidth;
    int startY = paint.rcPaint.top-1 + (-paint.rcPaint.top) % cellHeight;

    for (int x = startX; x < paint.rcPaint.right; x += cellWidth) {
        RECT line = paint.rcPaint;
        line.left = x;
        line.right = line.left + SC_PX(1, dpi);

        FillRect(hdc, &line, ctx->gridLnBrush);
    }

    for (int y = startY; y < paint.rcPaint.bottom; y += cellHeight) {
        RECT line = paint.rcPaint;
        line.top = y;
        line.bottom = line.top + SC_PX(1, dpi);

        FillRect(hdc, &line, ctx->gridLnBrush);
    }

    EndPaint(wnd, &paint);

    return 0;
}

static LRESULT CALLBACK OnDestroy(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct SheetCtrlCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);

    if (ctx) {
        if (ctx->gridBgBrush) DeleteObject(ctx->gridBgBrush);
        if (ctx->gridLnBrush) DeleteObject(ctx->gridLnBrush);

        SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)NULL);
        free(ctx);
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    WNDPROC handler;

    switch (msg) {
    case WM_PAINT: handler = OnPaint; break;
    case WM_DESTROY: handler = OnDestroy; break;
    default: handler = DefWindowProc; break;
    }

    return handler(wnd, msg, wParam, lParam);
}

HWND SCCreateSheetCtrl(HINSTANCE hInstance, HWND parent, RECT rect)
{
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(wc);
        wc.cbWndExtra = sizeof(LONG_PTR);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = WndClass;
        wc.lpfnWndProc = WndProc;

        SC_CHECK(
            RegisterClassEx(&wc),
            _T("Failed to register sheet control"));

        registered = TRUE;
    }

    HWND wnd = CreateWindow(
        WndClass, NULL, WS_VISIBLE | WS_CHILD,
        rect.left, rect.top,
        rect.right - rect.left, rect.bottom - rect.top,
        parent, NULL, hInstance, NULL);

    SC_CHECK(wnd, _T("Failed to create sheet control"));

    struct SheetCtrlCtx *ctx = calloc(1, sizeof(struct SheetCtrlCtx));
    SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)ctx);

    return wnd;
}
