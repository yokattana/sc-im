#pragma once

#include <windows.h>

#if (WINVER < 0x0605)

void SCCompatSetup(void);

#define GetDpiForWindow GetDpiForWindowStub

int GetDpiForWindowStub(HWND wnd);

#else

#define SCCompatSetup() do {} while (0)

#endif
