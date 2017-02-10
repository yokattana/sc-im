#include "main.h"
#include "compat.h"
#include "resource.h"

void SCAbort(LPCTSTR message)
{
    MessageBox(NULL, message, SC_TITLE, MB_OK | MB_ICONWARNING);
    ExitProcess(-1);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    SCCompatSetup();

    HWND wnd = SCCreateMainWnd(hInstance);

    ShowWindow(wnd, nCmdShow);
    UpdateWindow(wnd);

    HANDLE accel = LoadAccelerators(hInstance,
        MAKEINTRESOURCE(IDR_MAIN_ACCEL));

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(wnd, accel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}
