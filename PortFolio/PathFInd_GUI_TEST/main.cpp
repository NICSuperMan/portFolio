#include <cmath>
#include <cstdlib>
#include <windows.h>
#include "Render.h"
#include <time.h>
MSG msg;
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int nCmdShow)
{
	HWND hWnd;
	hInst = hInstance;
	srand((unsigned)time(NULL));
	RegisterWindow();
	hWnd = CreateWindowW(L"TEST", L"MyTest", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	makeWindow(hWnd, nCmdShow);
	while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
