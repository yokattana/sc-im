#pragma once

#include <windows.h>
#include <tchar.h>

#define SC_TITLE _T("Calculating Spreadsheet")
#define SC_PX(x, dpi) ((int)(x * (dpi / 96.0)))
#define SC_CHECK(ok, msg) do { if (!ok) SCAbort(msg); } while(0)

void SCAbort(LPCTSTR message);

HWND SCCreateMainWnd(HINSTANCE hInstance);
HWND SCCreateSheetCtrl(HINSTANCE hInstance, HWND parent, RECT rect);
