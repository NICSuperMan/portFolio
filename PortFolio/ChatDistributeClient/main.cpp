// ChatDistributionClient.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include <WinSock2.h>
#include <clocale>
#include <Parser.h>
#include <cmath>
#include "framework.h"
#include "Resource.h"
#include "ChatDistributionClient.h"

#define MAX_LOADSTRING 100
#define WM_REQUEST_INVALIDATE_RECV (WM_USER + 1)

// 전역 변수:
HINSTANCE hInst;
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HPEN g_hGridPen;
HBRUSH g_hTileBrush;
RECT g_MemDCRect;
HBITMAP g_hMemDCBitmap;
HBITMAP g_hMemDCBitmap_old;
HDC g_hMemDC;
bool g_MouseOnClick = false;
bool g_bFirstPaint = true;
int g_oldX;
int g_oldY;
HWND g_hWnd;

bool g_bRecvPacket = false;

int g_sectorFixelX;
int g_sectorFixelY;
int g_screenWidth;
int g_screenHeight;

constexpr int SECTOR_HEIGHT = 50;
constexpr int SECTOR_WIDTH = 50;

COLORREF* g_pColorRef;
int g_AlertNum;




LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    hInst = hInstance;
    MSG msg;
    HWND hWnd;
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_CHATDISTRIBUTECLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TEST";
    wcex.hIconSm = NULL;
    RegisterClassExW(&wcex);

    g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYSCREEN);


	PARSER psr = CreateParser(L"DistributionClientConfig.txt");
    g_AlertNum = GetValueINT(psr, L"ALERT_NUM");
	ReleaseParser(psr);
    
    g_pColorRef = new COLORREF[g_AlertNum];
    int offset = 255 / g_AlertNum;

    for (int i = 0; i < g_AlertNum; ++i)
    {
        g_pColorRef[i] = RGB(255, 255 - i * offset , 255 - i * offset);
    }

    hWnd = CreateWindowW(L"TEST", L"ChatDistributionClient", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, hInstance, nullptr);
    g_hWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    setlocale(LC_ALL, "");
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void RenderGrid(HDC hdc, int sectorWidth, int sectorHeigth)
{
    HPEN hOldPen = (HPEN)SelectObject(hdc, g_hGridPen);
    g_sectorFixelX = (int)std::floor((float)g_screenWidth / sectorWidth);
    g_sectorFixelY = (int)std::floor((float)g_screenHeight / sectorHeigth);

    for (int x = 0; x < SECTOR_WIDTH * g_sectorFixelX; x += g_sectorFixelX)
    {
        MoveToEx(hdc, x, 0, NULL);
        LineTo(hdc, x, g_screenHeight);
    }

    for (int y = 0; y < SECTOR_HEIGHT * g_sectorFixelY; y += g_sectorFixelY)
    {
        MoveToEx(hdc, 0, y, NULL);
        LineTo(hdc, g_screenWidth, y);
    }
    SelectObject(hdc, hOldPen);
}

Packet* g_pSectorInfoPacket = nullptr;

ChatDistributionClient* g_pChatClient = nullptr;

void RenderSectorInfo(HDC hdc)
{
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, g_hTileBrush);

    int pos = 0;
    for (int y = 0; y < SECTOR_HEIGHT * g_sectorFixelY; y += g_sectorFixelY)
    {
        for (int x = 0; x < SECTOR_WIDTH * g_sectorFixelX; x += g_sectorFixelX)
        {
            char idx;
            (*g_pSectorInfoPacket) >> idx;

            if (idx < 0)
            {
                LOG(L"ERROR", SYSTEM, TEXTFILE, L"(x,y) -> (%d,%d) PeopleCount : %d", x, y, idx);
                __debugbreak();
            }

            hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));

            if (idx >= g_AlertNum)
                SetDCBrushColor(hdc, RGB(0, 0, 0));
            else
                SetDCBrushColor(hdc, g_pColorRef[idx]);

            Rectangle(hdc, x + 1, y + 1, x + g_sectorFixelX + 1, y + g_sectorFixelY + 1);
            ++pos;
        }
    }
    SelectObject(hdc, hOldBrush);

    if (g_pSectorInfoPacket->DecrementRefCnt() == 0)
        PACKET_FREE(g_pSectorInfoPacket);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc = GetDC(hWnd);
    PAINTSTRUCT ps;
    switch (message)
    {
    case WM_CREATE:
    {
        g_hGridPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        g_hTileBrush = CreateSolidBrush(RGB(100, 100, 100));
        GetClientRect(hWnd, &g_MemDCRect);
        g_hMemDCBitmap = CreateCompatibleBitmap(hdc, g_MemDCRect.right, g_MemDCRect.bottom);
        g_hMemDC = CreateCompatibleDC(hdc); // 메모리 디바이스 컨텍스트 얻음
        ReleaseDC(hWnd, hdc);
        g_hMemDCBitmap_old = (HBITMAP)SelectObject(g_hMemDC, g_hMemDCBitmap); // 최초에 만든 메모리 비트맵 백업

        // 클라 객체 생성
        WCHAR ip[16];
        PARSER psr = CreateParser(L"DistributionClientConfig.txt");
        GetValueWSTR(psr, ip, _countof(ip), L"BIND_IP");
        g_pChatClient = new ChatDistributionClient
        {   FALSE,
            0,
            0,
            TRUE,
            ip,
            (USHORT)GetValueINT(psr,L"BIND_PORT"),
            GetValueUINT(psr,L"IOCP_WORKER_THREAD"),
            GetValueUINT(psr,L"IOCP_ACTIVE_THREAD"),
            1,
            (BYTE)GetValueINT(psr,L"PACKET_CODE"),
            (BYTE)GetValueINT(psr,L"PACKET_KEY"),
            GetValueUINT(psr,L"REQUEST_INTERVAL"),
            (DWORD)g_AlertNum 
        };
        g_pChatClient->Start();
    }
    break;
    case WM_PAINT:
    {
        HDC hdc = BeginPaint(hWnd, &ps);

        // 백버퍼 초기화
        PatBlt(g_hMemDC, 0, 0, g_MemDCRect.right, g_MemDCRect.bottom, WHITENESS);
        RenderGrid(g_hMemDC, SECTOR_WIDTH, SECTOR_HEIGHT);

        if (g_bRecvPacket)
        {
            RenderSectorInfo(g_hMemDC);
        }

        // 백버퍼에서 실제 화면으로 복사
        BitBlt(hdc, 0, 0, g_MemDCRect.right, g_MemDCRect.bottom, g_hMemDC, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        g_bRecvPacket = false;
     }
    break;
    case WM_LBUTTONDOWN:
        g_MouseOnClick = true;
        break;
    case WM_LBUTTONUP:
        g_MouseOnClick = false;
        break;
    case WM_DESTROY:
    {
        DeleteObject(g_hGridPen);
        DeleteObject(g_hTileBrush);
        delete g_pChatClient;
        PostQuitMessage(0);
    }
    break;
    case WM_REQUEST_INVALIDATE_RECV:
        g_bRecvPacket = true;
        g_pSectorInfoPacket = (Packet*)wParam;
        InvalidateRect(hWnd, nullptr, false);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    ReleaseDC(hWnd, hdc);
    return 0;
}
