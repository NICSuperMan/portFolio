#include <windows.h>
#include <map>
#include <cassert>
#include <time.h>
#include "VERTEX.h"
#include "Coordinate.h"
#include "ASTAR.h"
#include "Tile.h"
#pragma comment(lib,"winmm.lib")

extern Tile g_Tile;
extern MSG msg;
void ConvertMillisecondsToTime(DWORD* milliseconds, DWORD* days, DWORD* hours, DWORD* minutes, DWORD* seconds);

bool ASTAR::pathFind(HWND hWnd)
{
    openList_.insert(std::pair<Coordinate, VERTEX*>{start_, new VERTEX{ start_,dest_ }});
    while (!openList_.empty())
    {
       std::map<Coordinate,VERTEX*>::iterator current_vertex = std::min_element(openList_.begin(), openList_.end(), [](const auto& pair1, const auto& pair2) {
            return pair1.second->_F < pair2.second->_F;
            });

        VERTEX* current = current_vertex->second;
        if(current->_pos == dest_)
        {
            openList_.erase(current->_pos);
            closeList_.insert(std::pair<Coordinate, VERTEX*>{current->_pos, current});
            pCurrentVertex_ = current;//경로를 이은 선에 같이 연결되도록
            g_Tile[current->_pos] = DESTINATION;
            return true;//길찾기 성공
        }
        //방문처리(출발지는 중복색칠을 막기위해 표시안함)
        do
        {
            if (current->_pos == start_)
                break;
            if(current->_pos == dest_)
                break;

            g_Tile[current->_pos] = VISITED;
        } while (false);

		//현재 위치를 _close_list에 넣고 이동
        closeList_.insert(std::pair<Coordinate, VERTEX*>{current->_pos, current});
        openList_.erase(current->_pos);

        // 이동경로를 출력하기위해 현재 vertex를 포인터 멤버변수에 저장
        pCurrentVertex_ = current;

        // openList에 주위 8방향 노드 삽입
        for (int i = Coordinate::NORTH; i < Coordinate::TOTAL; ++i)
        {
            // 이미 openList나 closeList에 존재하는 경우
            do
            {
                Coordinate next_pos = current->_pos + Coordinate::_direction[i];
                if (g_Tile.index_outOf_range(next_pos))   break;

                STATE next_state = (STATE)g_Tile[next_pos];
                if (next_state == CANDIDATE && next_state != START) // openList에 존재하면
                {
					VERTEX* existing_vertex = openList_.find(next_pos)->second; // 비교하기 위해서 꺼냄
					double compare_G = getGHF::get_G(Coordinate{ next_pos }, current);
					if (compare_G < existing_vertex->_G)
					{
						existing_vertex->_G = compare_G;
						existing_vertex->_F = existing_vertex->_H + compare_G;
						existing_vertex->_parent_vertex = current;
					}
					break;
				}
                else if (next_state == VISITED && next_state != START)
                    break;
                else if (next_state == OBSTACLE)
                    break;
                // 중복도 되지 않앗고 이미 방문한 지점도 아니기 때문에 생성
                if (next_state == START)  
                    continue;
                VERTEX* temp = new VERTEX{next_pos,dest_,current };
                g_Tile[temp->_pos] = CANDIDATE;
                openList_.insert(std::pair<Coordinate, VERTEX*>(temp->_pos, temp));
            } while (false);
       }
    }
    return false;
}
void ASTAR::drawParentLine(HDC hdc)
{
    // 출발점이면 return
    // openList 순회
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(139, 69, 19)); // 갈색 선 생성
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    for (auto iter = openList_.begin(); iter != openList_.end(); ++iter)
    {
        // 맨처음에 출발점이 openList에 있을때
        if (iter->second->_parent_vertex == nullptr)
            continue;

        Coordinate&& parent_midPoint = make_midPoint(iter->second->_parent_vertex);
        Coordinate&& current_midPoint = make_midPoint(iter->second);
        Coordinate&& vector_to_parent = (parent_midPoint - current_midPoint);
        MoveToEx(hdc, current_midPoint._x, current_midPoint._y, NULL);
        LineTo(hdc, current_midPoint._x + vector_to_parent._x, current_midPoint._y + vector_to_parent._y);
    }
    // close_list 순회
    for (auto&& iter = closeList_.begin(); iter != closeList_.end(); ++iter)
    {
        // closeList에 존재하는 출발점은 그리지않음. (맨처음을 제외하면 항상 closeList에 출발점이 존재할것임)
        if (iter->second->_parent_vertex == nullptr)
            continue;

        Coordinate&& current_midPoint = make_midPoint(iter->second);
        Coordinate&& parent_midPoint = make_midPoint(iter->second->_parent_vertex);
        Coordinate&& vector_to_parent = (parent_midPoint - current_midPoint) / 2;
        MoveToEx(hdc, current_midPoint._x, current_midPoint._y, NULL);
        LineTo(hdc, current_midPoint._x + vector_to_parent._x, current_midPoint._y + vector_to_parent._y);
    }
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

ASTAR::ASTAR()
    :PATHFINDER()// 기본인자들 전부 들어가도록
{
}

