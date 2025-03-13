#pragma once
#include <Windows.h>
#include <map>
extern HINSTANCE hInst;
void RegisterWindow(void);
void makeWindow(HWND hWnd, int nCmdShow);
int APIENTRY start(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void RenderGrid(HDC hdc);
void RenderObstacle(HDC hdc);



