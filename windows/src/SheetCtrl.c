#include "main.h"

#define CELL_WIDTH 65
#define CELL_HEIGHT 20

#define GRID_BGCOLOR RGB(0xFF, 0xFF, 0xFF)
#define GRID_LNCOLOR RGB(0xDD, 0xDD, 0xDD)

#define CELLRECT(c,r,w,h) {c*w,r*h,c*w+w,r*h+h}

static const LPCTSTR WndClass = _T("SCSheetCtrl");

struct SheetCtrlCtx {
    HBRUSH gridBgBrush;
    HBRUSH gridLnBrush;
    HFONT titleFont;
};

static LPTSTR GetColName(LPTSTR buf, size_t len, int col)
{
    if (!buf || len < 1) return NULL;
    if (len == 1) { *buf = 0; return buf; }

    buf[len-1] = 0;
    buf[len-2] = 'A' + (col-1) % 26;

    int remainder = (col-1) / 26;
    LPTSTR start = buf + len-2;
    while (start > buf && remainder > 0) {
        *(--start) = 'A' + remainder % 26;
        remainder /= 26;
    }

    return start;
}

static LRESULT CALLBACK OnPaint(HWND wnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    struct SheetCtrlCtx *ctx = (void*)GetWindowLongPtr(wnd, GWLP_USERDATA);
    SC_CHECK(ctx, _T("Failed to get sheet control context"));

    if (!ctx->gridBgBrush) ctx->gridBgBrush = CreateSolidBrush(GRID_BGCOLOR);
    if (!ctx->gridLnBrush) ctx->gridLnBrush = CreateSolidBrush(GRID_LNCOLOR);

    if (!ctx->titleFont) {
        NONCLIENTMETRICS ncm = {0};
        ncm.cbSize = sizeof(ncm);

        SC_CHECK(
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0),
            _T("Failed to get metrics info"));

        LOGFONT titleLf = ncm.lfMessageFont;
        titleLf.lfWeight = 600;

        ctx->titleFont = CreateFontIndirect(&titleLf);
        SC_CHECK(ctx->titleFont, _T("Failed to create title font"));
    }

    PAINTSTRUCT paint;
    HDC hdc = BeginPaint(wnd, &paint);
    if (!hdc)
        return 0;

    UINT dpi = GetDpiForWindow(wnd);
    int cellWidth = SC_PX(CELL_WIDTH, dpi);
    int cellHeight = SC_PX(CELL_HEIGHT, dpi);

    FillRect(hdc, &paint.rcPaint, ctx->gridBgBrush);

    RECT range;
    range.left   = paint.rcPaint.left / cellWidth;
    range.top    = paint.rcPaint.top  / cellHeight;
    range.right  = paint.rcPaint.right  / cellWidth  + 1;
    range.bottom = paint.rcPaint.bottom / cellHeight + 1;

    for (int col = range.left; col <= range.right; col++) {
        RECT line = paint.rcPaint;
        line.left  = (col + 1) * cellWidth - 1;
        line.right = line.left + SC_PX(1, dpi);

        FillRect(hdc, &line, ctx->gridLnBrush);
    }

    for (int row = range.top; row <= range.bottom; row++) {
        RECT line = paint.rcPaint;
        line.top    = (row + 1) * cellHeight - 1;
        line.bottom = line.top + SC_PX(1, dpi);

        FillRect(hdc, &line, ctx->gridLnBrush);
    }

    SelectObject(hdc, ctx->titleFont);

    for (int col = max(1, range.left); col <= range.right; col++) {
        TCHAR buf[8];
        LPTSTR text = GetColName(buf, _countof(buf), col);
        RECT rect = CELLRECT(col, 0, cellWidth, cellHeight);
        DrawText(hdc, text, -1, &rect, DT_CENTER);
    }

    for (int row = max(1, range.top); row <= range.bottom; row++) {
        TCHAR buf[16];
        _stprintf_s(buf, _countof(buf), _T("%d"), row);
        RECT rect = CELLRECT(0, row, cellWidth, cellHeight);
        DrawText(hdc, buf, -1, &rect, DT_CENTER);
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
        if (ctx->titleFont) DeleteObject(ctx->titleFont);

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
