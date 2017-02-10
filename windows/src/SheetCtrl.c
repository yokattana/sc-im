#include "main.h"
#include "compat.h"

#define CELL_WIDTH 65
#define CELL_HEIGHT 20

#define GRID_BGCOLOR RGB(0xFF, 0xFF, 0xFF)
#define GRID_LNCOLOR RGB(0xDD, 0xDD, 0xDD)

static const LPCTSTR WndClass = _T("SCSheetCtrl");

static HBRUSH gridBgBrush = NULL;
static HBRUSH gridLnBrush = NULL;

static LRESULT CALLBACK OnPaint(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    if (!gridBgBrush) gridBgBrush = CreateSolidBrush(GRID_BGCOLOR);
    if (!gridLnBrush) gridLnBrush = CreateSolidBrush(GRID_LNCOLOR);

    PAINTSTRUCT paint;
    HDC hdc = BeginPaint(wnd, &paint);
    if (!hdc)
        return 0;

    UINT dpi = GetDpiForWindow(wnd);
    int cellWidth = SC_PX(CELL_WIDTH, dpi);
    int cellHeight = SC_PX(CELL_HEIGHT, dpi);

    FillRect(hdc, &paint.rcPaint, gridBgBrush);

    int startX = paint.rcPaint.left-1 + (-paint.rcPaint.left) % cellWidth;
    int startY = paint.rcPaint.top-1 + (-paint.rcPaint.top) % cellHeight;

    for (int x = startX; x < paint.rcPaint.right; x += cellWidth) {
        RECT line = paint.rcPaint;
        line.left = x;
        line.right = line.left + SC_PX(1, dpi);

        FillRect(hdc, &line, gridLnBrush);
    }

    for (int y = startY; y < paint.rcPaint.bottom; y += cellHeight) {
        RECT line = paint.rcPaint;
        line.top = y;
        line.bottom = line.top + SC_PX(1, dpi);

        FillRect(hdc, &line, gridLnBrush);
    }

    EndPaint(wnd, &paint);

    return 0;
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    WNDPROC handler;

    switch (msg) {
    case WM_PAINT: handler = OnPaint; break;
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

    return wnd;
}
