#include "compat.h"
#include "main.h"

#if (WINVER < 0x0605)

#undef GetDpiForWindow

#define MONITOR_DPI_TYPE int
#define PROCESS_PER_MONITOR_DPI_AWARE 2

HRESULT (*GetDpiForMonitor)(
    HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);

int (*GetDpiForWindow)(HWND hwnd);

void SCCompatSetup()
{
    HINSTANCE user32 = LoadLibrary(_T("User32.dll"));
    if (user32) {
        GetDpiForMonitor = (void*)GetProcAddress(user32, "GetDpiForMonitor");
        GetDpiForWindow = (void*)GetProcAddress(user32, "GetDpiForWindow");
    }
}

int GetDpiForWindowStub(HWND wnd)
{
    if (GetDpiForWindow)
        return GetDpiForWindow(wnd);

    if (GetDpiForMonitor) {
        HMONITOR monitor = MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY);
        if (monitor) {
            INT dpiX, dpiY;
            HRESULT ret = GetDpiForMonitor(
                monitor,
                PROCESS_PER_MONITOR_DPI_AWARE,
                &dpiX, &dpiY);

            if (ret) return dpiX;
        }
    }

    return 96;
}

#endif