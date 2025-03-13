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
            pCurrentVertex_ = current;//��θ� ���� ���� ���� ����ǵ���
            g_Tile[current->_pos] = DESTINATION;
            return true;//��ã�� ����
        }
        //�湮ó��(������� �ߺ���ĥ�� �������� ǥ�þ���)
        do
        {
            if (current->_pos == start_)
                break;
            if(current->_pos == dest_)
                break;

            g_Tile[current->_pos] = VISITED;
        } while (false);

		//���� ��ġ�� _close_list�� �ְ� �̵�
        closeList_.insert(std::pair<Coordinate, VERTEX*>{current->_pos, current});
        openList_.erase(current->_pos);

        // �̵���θ� ����ϱ����� ���� vertex�� ������ ��������� ����
        pCurrentVertex_ = current;

        // openList�� ���� 8���� ��� ����
        for (int i = Coordinate::NORTH; i < Coordinate::TOTAL; ++i)
        {
            // �̹� openList�� closeList�� �����ϴ� ���
            do
            {
                Coordinate next_pos = current->_pos + Coordinate::_direction[i];
                if (g_Tile.index_outOf_range(next_pos))   break;

                STATE next_state = (STATE)g_Tile[next_pos];
                if (next_state == CANDIDATE && next_state != START) // openList�� �����ϸ�
                {
					VERTEX* existing_vertex = openList_.find(next_pos)->second; // ���ϱ� ���ؼ� ����
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
                // �ߺ��� ���� �ʾѰ� �̹� �湮�� ������ �ƴϱ� ������ ����
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
    // ������̸� return
    // openList ��ȸ
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(139, 69, 19)); // ���� �� ����
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    for (auto iter = openList_.begin(); iter != openList_.end(); ++iter)
    {
        // ��ó���� ������� openList�� ������
        if (iter->second->_parent_vertex == nullptr)
            continue;

        Coordinate&& parent_midPoint = make_midPoint(iter->second->_parent_vertex);
        Coordinate&& current_midPoint = make_midPoint(iter->second);
        Coordinate&& vector_to_parent = (parent_midPoint - current_midPoint);
        MoveToEx(hdc, current_midPoint._x, current_midPoint._y, NULL);
        LineTo(hdc, current_midPoint._x + vector_to_parent._x, current_midPoint._y + vector_to_parent._y);
    }
    // close_list ��ȸ
    for (auto&& iter = closeList_.begin(); iter != closeList_.end(); ++iter)
    {
        // closeList�� �����ϴ� ������� �׸�������. (��ó���� �����ϸ� �׻� closeList�� ������� �����Ұ���)
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
    :PATHFINDER()// �⺻���ڵ� ���� ������
{
}

