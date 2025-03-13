#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <time.h>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <algorithm>
#include "Render.h"
#include "Tile.h"
#include "PATHFINDER.h"
#include "ASTAR.h"
#include "JPS.h"
#include "VERTEX.h"

#pragma warning (disable : 4244)
// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
BOOL g_bClick;
HPEN g_hPen;
HBITMAP g_hMemDCBitmap;
HBITMAP g_hMemDCBitmap_old;
HDC g_hMemDC;
RECT g_MemDCRect;
bool g_isPrintPos = false;
Coordinate cursor_tile_index;
HBRUSH g_hTileBrush;
HPEN g_hGridPen;
Tile g_Tile;
bool g_bErase = false;
bool g_bDrag = false;
bool g_drawUI = false;

// 방문지 후보지 색칠 여부를 결정하는 변수로 3번 키다운에 의해 스위칭되며 render obstacle에서 사용된다.
bool g_draw_CANDIDATE_VISITED = true;

bool is_Astar = true;
ASTAR g_ASTAR;
JPS g_JPS;
PATHFINDER* pathfinder = &g_JPS;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void RenderGrid(HDC hdc);
void RenderUI(HDC hdc);

Coordinate g_render_start_point;
int g_scroll_up = 0;

void RegisterWindow(void)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(hInst, NULL);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TEST";
    wcex.hIconSm = NULL;
    RegisterClassExW(&wcex);
}
void makeWindow(HWND hWnd, int nCmdShow)
{
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;
    switch (message)
    {
    case WM_LBUTTONDOWN:
        g_bDrag = true;
        {
            Coordinate tile_index= Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) } / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
            if (g_Tile.index_outOf_range(tile_index))
                break;
            if(g_Tile[tile_index] == OBSTACLE)
                g_bErase = true;
            else
                g_bErase = false;
        }
        break;
    case WM_LBUTTONUP:
        g_bDrag = false;
        break;
    case WM_LBUTTONDBLCLK:
    {
        if (!g_ASTAR.isSetStartPoint_)
        {
            PATHFINDER::start_ = Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) } / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
            g_Tile[PATHFINDER::start_] = START;
        }
        else
        {
            for (auto iter = PATHFINDER::openList_.begin(); iter != g_ASTAR.openList_.end();)
            {
				g_Tile[iter->first] = NOSTATE;
                iter = g_ASTAR.openList_.erase(iter);
            }

            for (auto iter = PATHFINDER::closeList_.begin(); iter != g_ASTAR.closeList_.end();)
            {
                PATHFINDER::pCurrentVertex_ = nullptr;
                g_Tile[iter->first] = NOSTATE;
                iter = PATHFINDER::closeList_.erase(iter);
            }
            g_Tile[PATHFINDER::start_] = NOSTATE;
            PATHFINDER::start_ = Coordinate{ 0,0 };
        }

        g_ASTAR.isSetStartPoint_ = !g_ASTAR.isSetStartPoint_;
        InvalidateRect(hWnd, NULL, false);
    }
    break;

    case WM_RBUTTONDBLCLK:
    {
        if (!g_ASTAR.isSetDest)
        {
            g_ASTAR.dest_ = Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) } / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
            g_Tile[g_ASTAR.dest_] = DESTINATION;
        }
        else
        {
            for (auto iter = g_ASTAR.openList_.begin(); iter != g_ASTAR.openList_.end();)
            {
				g_Tile[iter->first] = NOSTATE;
                iter = g_ASTAR.openList_.erase(iter);
            }

            for (auto iter = g_ASTAR.closeList_.begin(); iter != g_ASTAR.closeList_.end();)
            {
                g_Tile[iter->first] = NOSTATE;
                iter = g_ASTAR.closeList_.erase(iter);
            }
            g_Tile[g_ASTAR.dest_] = NOSTATE;
            g_ASTAR.pCurrentVertex_ = nullptr;
            g_ASTAR.dest_ = Coordinate{ 0,0 };
        }

        g_ASTAR.isSetDest = !g_ASTAR.isSetDest;
        InvalidateRect(hWnd, NULL, false);
    }
    break;

    case WM_MOUSEMOVE:
    {
        if (g_bDrag)
        {
            cursor_tile_index = Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) } / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
            if (g_Tile.index_outOf_range(cursor_tile_index))    break;
            g_Tile[cursor_tile_index] = !g_bErase;
            InvalidateRect(hWnd, NULL, false);
        }
    }
    break;
    case WM_MOUSEWHEEL:
	{
		// 땡긴 위치의 x, y 좌표 구하기
        Coordinate original_pos = Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        Coordinate zoomPos = original_pos / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
        int zoomDelta = GET_WHEEL_DELTA_WPARAM(wParam);

        if (zoomDelta < 0)
        {
			if (g_scroll_up > 0)
				g_scroll_up -= 20;
		}
        if (zoomDelta > 0)
        {
            if(g_scroll_up <= 60)
                g_scroll_up += 20;
        }
        
        g_render_start_point = zoomPos - original_pos / (Tile::_GRID_SIZE + g_scroll_up);
        if(g_render_start_point._x < 0)
			g_render_start_point._x = 0;

        if(g_render_start_point._y < 0)
            g_render_start_point._y = 0;
		InvalidateRect(hWnd, NULL, false);
	}
	break;
    case WM_CREATE:
    {
        HDC hdc = GetDC(hWnd);
        g_hGridPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        g_hTileBrush = CreateSolidBrush(RGB(100, 100, 100));
        GetClientRect(hWnd, &g_MemDCRect);
        g_hMemDCBitmap = CreateCompatibleBitmap(hdc, g_MemDCRect.right, g_MemDCRect.bottom);
        g_hMemDC = CreateCompatibleDC(hdc); // 메모리 디바이스 컨텍스트 얻음
        ReleaseDC(hWnd, hdc);
        g_hMemDCBitmap_old = (HBITMAP)SelectObject(g_hMemDC, g_hMemDCBitmap); // 최초에 만든 메모리 비트맵 백업
    }
    break;
    case WM_PAINT:
    {
        PatBlt(g_hMemDC, 0, 0, g_MemDCRect.right, g_MemDCRect.bottom, WHITENESS);
        hdc = BeginPaint(hWnd, &ps);
        RenderGrid(g_hMemDC);
        RenderObstacle(g_hMemDC);
        RenderUI(g_hMemDC);
        if (pathfinder->_isStart())
        {
			pathfinder->drawPolyLine(g_hMemDC);
			g_ASTAR.drawParentLine(g_hMemDC);
			if (is_Astar)
			{
                //if (g_draw_CANDIDATE_VISITED);
			}
        }
        BitBlt(hdc, 0, 0, g_MemDCRect.right, g_MemDCRect.bottom, g_hMemDC, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_SIZE:
    {
        SelectObject(g_hMemDC, g_hMemDCBitmap_old);
        DeleteObject(g_hMemDC);
        DeleteObject(g_hMemDCBitmap);

        HDC hdc = GetDC(hWnd);
        GetClientRect(hWnd, &g_MemDCRect);
        g_hMemDCBitmap = CreateCompatibleBitmap(hdc, g_MemDCRect.right, g_MemDCRect.bottom);
        g_hMemDC = CreateCompatibleDC(hdc);
        ReleaseDC(hWnd, hdc);
        g_hMemDCBitmap_old = (HBITMAP)SelectObject(g_hMemDC, g_hMemDCBitmap);
    }
    break;
    case WM_DESTROY:
        SelectObject(g_hMemDC, g_hMemDCBitmap); // 이건 왜?
        DeleteObject(g_hMemDC);
        DeleteObject(g_hMemDCBitmap);
        DeleteObject(g_hTileBrush);
        DeleteObject(g_hGridPen);
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
    {
		Coordinate original_pos = Coordinate{ GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        Coordinate zoomPos = original_pos / (Tile::_GRID_SIZE + g_scroll_up) + g_render_start_point;
        if(g_Tile.index_outOf_range(zoomPos))    break;
        switch (wParam)
        {
        case VK_SHIFT:
            g_drawUI = !g_drawUI;
			InvalidateRect(hWnd, NULL, false);
            break;
        case VK_CONTROL:
            g_isPrintPos = !g_isPrintPos;
            InvalidateRect(hWnd, NULL, false);
            break;
        case VK_TAB:
            if (g_ASTAR._isStart())
            {
                pathfinder->pathFind(hWnd);
                InvalidateRect(hWnd, NULL, false);
            }
            break;
        case VK_UP:
            if (g_render_start_point._y > 0)
                --g_render_start_point._y;
            InvalidateRect(hWnd, NULL, false);
            break;
		case VK_DOWN:
            if(g_render_start_point._y < Tile::GRID_HEIGHT - 1)
				++g_render_start_point._y;
            InvalidateRect(hWnd, NULL, false);
            break;
        case VK_LEFT:
			if (g_render_start_point._x > 0)
				--g_render_start_point._x;
			InvalidateRect(hWnd, NULL, false);
			break;
        case VK_RIGHT:
			if (g_render_start_point._x < Tile::GRID_WIDTH - 1)
				++g_render_start_point._x;
            InvalidateRect(hWnd, NULL, false);
			break;
        case VK_SPACE:
            g_Tile.set_map_and_setting(hWnd);
			InvalidateRect(hWnd, NULL, false);
			UpdateWindow(hWnd);
            break;
        case 0x31://1번키
            pathfinder->reset();
            g_Tile.clear_tile(true, true, true, true, true);
            InvalidateRect(hWnd, NULL, false);
            break;
        case 0x32: // 출발지 도착지 장애물 남겨놓고 모든것을 초기화
        {
            g_Tile.clear_tile(false, true, true, false,true);
			PATHFINDER::pCurrentVertex_ = nullptr;
			InvalidateRect(hWnd, NULL, false);

			if (!PATHFINDER::openList_.empty())
			{
				for (auto iter = PATHFINDER::openList_.begin(); iter != PATHFINDER::openList_.end(); )
				{
					delete iter->second;
					iter = PATHFINDER::openList_.erase(iter);
				}
			}

			if (!PATHFINDER::closeList_.empty())
			{
				for (auto iter = PATHFINDER::closeList_.begin(); iter != PATHFINDER::closeList_.end(); )
				{
					delete iter->second;
					iter = PATHFINDER::closeList_.erase(iter);
				}
			}
			break;
		}
        case 0x33: // 방문지, 후보지 색칠 켜고 끄기
        {	
            g_draw_CANDIDATE_VISITED = !g_draw_CANDIDATE_VISITED;
			InvalidateRect(hWnd, NULL, false);
		}   
        break;
        case 0x34: // 확대 축소로 바뀐 시점을 초기시점으로 복구
        {
            g_scroll_up = 0;
            g_render_start_point = Coordinate{ 0,0 };
            InvalidateRect(hWnd, NULL, false);  
		}
        break;
        case 0x35:
            //g_Tile.reappearnce_all_1s_tile(hWnd);   
            //g_Tile.polling(hWnd);
            g_Tile.make_map();
            InvalidateRect(hWnd, NULL, false);
            break;
        case 0x38:
            g_Tile.make_map_render(hWnd);
            InvalidateRect(hWnd, NULL, false);
            break;
        case 0x36:
            pathfinder->test(hWnd);
			InvalidateRect(hWnd, NULL, false);
			break;
        case 0x37:
            if(is_Astar)
			{
				pathfinder = &g_JPS;
				is_Astar = false;
			}
			else
			{
				pathfinder = &g_ASTAR;
				is_Astar = true;
			}
			InvalidateRect(hWnd, NULL, false);
            break;
        default:
            break;
        }
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
void RenderGrid(HDC hdc)
{
    HPEN hOldPen = (HPEN)SelectObject(hdc, g_hGridPen);

    int iX = 0;
    int iY = 0;

    for (int x = 0; x <= Tile::GRID_WIDTH; ++x)
    {
        MoveToEx(hdc, iX, 0, NULL);
        LineTo(hdc, iX, (Tile::GRID_HEIGHT - g_render_start_point._y) * (Tile::_GRID_SIZE + g_scroll_up));
        iX += (Tile::_GRID_SIZE + g_scroll_up);
    }

    for (int y = 0; y <= Tile::GRID_HEIGHT; ++y)
    {
        MoveToEx(hdc, 0, iY, NULL);
        LineTo(hdc,(Tile::GRID_WIDTH - g_render_start_point._x) * (Tile::_GRID_SIZE + g_scroll_up), iY);
        iY += (Tile::_GRID_SIZE + g_scroll_up);
    }
    SelectObject(hdc, hOldPen);
}
void RenderObstacle(HDC hdc)
{

    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, g_hTileBrush);

    int iX = 0;
    int iY = 0;
    SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int y = 0; y < Tile::GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < Tile::GRID_WIDTH; ++x)
        {
            Coordinate pos{ x,y };
            switch ((STATE)g_Tile[pos])
            {
		    	case OBSTACLE:
					hOldBrush = (HBRUSH)SelectObject(hdc, g_hTileBrush);
					break;
				case START:
					hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hdc, RGB(0, 255, 0));
					break;
				case DESTINATION:
					hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hdc, RGB(255, 0, 0));
					break;
				case CANDIDATE: // 후보지
					hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hdc, RGB(0, 0, 255)); // BLUE
					break;
				case VISITED: // 방문지
					hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hdc, RGB(255, 0, 255)); // MAGENTA
					break;
                case SEARCHED: // JPS에서의 노드 탐색지
					hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
					SetDCBrushColor(hdc, RGB(255, 255, 0)); // YELLOW
					break;
                default: //NOSTATE
                    break;
            }
            int extended_grid_size = Tile::_GRID_SIZE + g_scroll_up;
            Coordinate render_pixel = (pos - g_render_start_point) * extended_grid_size;
            // 무상태가 아닌 경우에만 그린다.
            switch (g_Tile[pos])
            {
            case NOSTATE:
                break;
			case CANDIDATE:
			case VISITED:
                if (!g_draw_CANDIDATE_VISITED)
                    break;
            default:
				Rectangle(hdc, render_pixel._x + 1, render_pixel._y + 1, render_pixel._x + extended_grid_size + 1, render_pixel._y + extended_grid_size + 1);
                break;
            }


            if (g_draw_CANDIDATE_VISITED && g_scroll_up >= 60 && g_Tile[pos] != NOSTATE && g_Tile[pos] != OBSTACLE)
            {
                bool is_exist_in_openList = false;
                bool is_exist_in_closeList = false;
                do
                {
                    if (!g_isPrintPos)
                        break;

                    std::map<Coordinate, VERTEX*,pointCompare>* pMap = nullptr;

                    if (g_ASTAR.openList_.find(pos) != g_ASTAR.openList_.end())
                    {
                        is_exist_in_openList = true;
                        pMap = &pathfinder->openList_;
                    }
                    else if(g_ASTAR.closeList_.find(pos) != g_ASTAR.closeList_.end())
                    {
                        is_exist_in_closeList = true;
                        pMap = &pathfinder->closeList_;
                    }

                    // 둘다 없으면
                    if (g_Tile[pos] != SEARCHED && !is_exist_in_openList&& !is_exist_in_closeList)
                        break;
					HFONT hFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
					SelectObject(hdc, hFont);
					SetTextAlign(hdc, TA_LEFT);
					WCHAR TEMP[MAX_PATH];
					SetBkMode(hdc, TRANSPARENT);
					SetTextColor(hdc, RGB(0, 0, 0)); // 글자색 변경
                    swprintf_s(TEMP, _countof(TEMP), L"(x,y):(%d,%d)", pos._x, pos._y);
                    TextOut(hdc, render_pixel._x + 1, render_pixel._y + 1, TEMP, lstrlen(TEMP));
                    if(g_Tile[pos] == SEARCHED)
                        continue;
                    swprintf_s(TEMP, _countof(TEMP), L"G : %.3f", (*pMap)[pos]->_G);
                    TextOut(hdc, render_pixel._x + 1, render_pixel._y + 13, TEMP, lstrlen(TEMP));
                    swprintf_s(TEMP, _countof(TEMP), L"H : %d", (*pMap)[pos]->_H);
                    TextOut(hdc, render_pixel._x + 1, render_pixel._y + 25, TEMP, lstrlen(TEMP));
                    swprintf_s(TEMP, _countof(TEMP), L"F : %.5f", (*pMap)[pos]->_F);
                    TextOut(hdc, render_pixel._x + 1, render_pixel._y + 37, TEMP, lstrlen(TEMP));
					DeleteObject(hFont);
                } while (false);
            }
        }
    }
    SelectObject(hdc, hOldBrush);
}
void RenderUI(HDC hdc)
{
	HFONT hFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, 10, 10, L"UI 표시 : Shift키로 ON/OFF", lstrlen(L"UI 표시 : Shift키로 ON/OFF"));
    if (g_drawUI)
    {
		WCHAR TEMP[MAX_PATH];
        TextOut(hdc,10,22,L"마우스 왼쪽 더블클릭으로 시작지 설정 해제",lstrlen(L"1. 마우스 왼쪽 더블클릭으로 시작지 설정 해제"));
		TextOut(hdc, 10, 34, L"마우스 오른쪽 더블클릭으로 목적지 설정 해제", lstrlen(L"마우스 오른쪽 더블클릭으로 목적지 설정 해제"));
		TextOut(hdc, 10, 46, L"탭키로 경로 탐색", lstrlen(L"탭키로 경로 탐색"));
		TextOut(hdc, 10, 58, L"1번키로 다지움", lstrlen(L"1번키로 다지움"));
		TextOut(hdc, 10, 70, L"2번키로 방문지, 후보지 지움", lstrlen(L"2번키로 방문지, 후보지 지움"));
        TextOut(hdc, 10, 82, L"3번키로 방문지,후보지 색칠 켜고 끄기", lstrlen(L"3번키로 방문지,후보지 색칠 켜고 끄기"));
		TextOut(hdc, 10, 94, L"4번키로 초기시점 복구", lstrlen(L"4번키로 초기시점 복구"));
        TextOut(hdc, 10, 106, L"5번키로 맵 만들기", lstrlen(L"5번키로 맵 만들기"));
        TextOut(hdc, 10, 118, L"6번키로 맵 만들기(렌더링)", lstrlen(L"6번키로 맵 만들기(렌더링)"));
        swprintf_s(TEMP,_countof(TEMP),L"7번키로 길찾기 모드전환 현재 모드 : %s",is_Astar ? L"Astar" : L"JPS");
        TextOut(hdc, 10, 130, TEMP, lstrlen(TEMP));
		TextOut(hdc, 10, 142, L"방향키로 한칸씩 이동", lstrlen(L"방향키로 한칸씩 이동"));
		if (g_ASTAR.isSetStartPoint_)
		{
			swprintf_s(TEMP, _countof(TEMP), L"출발 좌표 (%d,%d)", g_ASTAR.start_._x, g_ASTAR.start_._y);
			TextOut(hdc, 10, 154, TEMP, lstrlen(TEMP));
		}
		if (g_ASTAR.isSetDest)
		{
			swprintf_s(TEMP, _countof(TEMP), L"도착 좌표 (%d,%d)", g_ASTAR.dest_._x, g_ASTAR.dest_._y);
            TextOut(hdc, 10, 166, TEMP, lstrlen(TEMP));
		}
    }
   	DeleteObject(hFont);

}
