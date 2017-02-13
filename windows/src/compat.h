#pragma once

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#ifndef _stprintf_s
# define _stprintf_s(b, n, ...) \
      do { _sntprintf(b, n, __VA_ARGS__); b[n-1] = 0; } while(0)
#endif

#if (WINVER < 0x0605)

void SCCompatSetup(void);

#define GetDpiForWindow GetDpiForWindowStub

int GetDpiForWindowStub(HWND wnd);

#else

#define SCCompatSetup() do {} while (0)

#endif
