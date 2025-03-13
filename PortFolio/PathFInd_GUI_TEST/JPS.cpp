#include <windows.h> 
#include <map>
#include <cassert>
#include <cmath>
#include <utility>
#include "VERTEX.h"
#include "Coordinate.h"
#include "PATHFINDER.h"
#include "Tile.h"
#include "JPS.h"
//#define SHOW
constexpr BYTE is_not_dir = 0b11111111;
constexpr BYTE dest = 0b11111110;
extern Tile g_Tile;
using pair = std::pair<Coordinate, VERTEX*>;
extern MSG msg;
Tile temp_tile;

JPS::JPS()
	: PATHFINDER() {}

bool JPS::pathFind(HWND hWnd)
{
	int i = 0;
	temp_tile = g_Tile;
	bool _is_first_search = true;
	openList_.insert(pair{ start_, new VERTEX{ start_,dest_,nullptr,is_not_dir,is_not_dir } });
	while (!openList_.empty())
	{
		//_F값이 가장 작은 노드를 오픈리스트에서 뽑기
		VERTEX* pCurrent = std::min_element(openList_.begin(), openList_.end(), [](const auto& pair1, const auto& pair2) {
			return pair1.second->_F < pair2.second->_F;
			})->second;

		Coordinate& temp = pCurrent->_pos;
		pCurrentVertex_ = pCurrent;
		closeList_.insert(pair{ pCurrent->_pos, pCurrent });
		openList_.erase(pCurrent->_pos);
		if (_is_first_search || pCurrent->_pos == dest_)
		{
			if (!_is_first_search)
			{
				pCurrentVertex_ = pCurrent;//경로를 이은 선에 같이 연결되도록
				g_Tile[pCurrent->_pos] = DESTINATION;
				{
					try
					{
						//int obs_after = g_Tile.count_obs();
						//if (obs_prev != obs_after)
							//throw 1;
						for (auto& pair : openList_)
						{
							for (auto& pair2 : closeList_)
							{
								if (pair.second->_pos == pair2.second->_pos)
									throw 2;
								if (g_Tile[pair.second->_pos] != CANDIDATE || g_Tile[pair2.second->_pos] != VISITED)
								{
									if (pair.second->_pos == start_ || pair2.second->_pos == start_) continue;
									if (pair.second->_pos == dest_ || pair2.second->_pos == dest_) continue;
									throw 3;
								}
							}
						}
					}
					catch (int exptNum)
					{
						FILE* fp;
						fopen_s(&fp, "error_map.txt", "wb");
						if (!fwrite(fp, sizeof(temp_tile._tile), 1, fp))
							throw;
						fclose(fp);
						fopen_s(&fp, "error_map_setting.txt", "wb");
						char temp[256];
						sprintf_s(temp, _countof(temp), "%d,%d,%d,%d", PATHFINDER::start_._x, PATHFINDER::start_._y, PATHFINDER::dest_._x, PATHFINDER::dest_._y);
						fwrite(temp, sizeof(temp), 1, fp);
						fclose(fp);
						WCHAR TEMP[100];
						if(exptNum == 1)
							swprintf_s(TEMP, L"장애물 손상");
						else if(exptNum == 2)
							swprintf_s(TEMP, L"오픈 리스트 클로즈 리스트 겹치는 항목 존재");
						else if(exptNum == 3)
							swprintf_s(TEMP, L"오픈, 클로즈 노드중 화면에 렌더링 되지 않는 놈 존재");
						MessageBox(hWnd, L"error", L"error_code", MB_OK);
					}
					return true;
				}
			}
		}
		else
			g_Tile[pCurrent->_pos] = VISITED;

		BYTE forced_dir;
		if (_is_first_search)
		{
			_is_first_search = false;
			Coordinate next_vertex;
			for (BYTE i = Direction::NORTH; i < Direction::TOTAL; ++i)
			{
				Coordinate&& temp = pCurrent->_pos + Coordinate::_direction[i];
				if(is_dir_diagonal(i))
					search_diagonal(pCurrent, temp, (Direction)i, hWnd);
				else
				{
					if (search_linear(pCurrent,temp, &next_vertex, (Direction)i, &forced_dir, hWnd))
					{
						if (g_Tile[next_vertex] != VISITED && g_Tile[next_vertex] != CANDIDATE)
						{
							g_Tile[next_vertex] = (char)CANDIDATE;
							openList_.insert(pair{ next_vertex, new VERTEX{ next_vertex, dest_, pCurrent,i,forced_dir} });
						}
					}
				}
			}
			continue;
		}
		// 만약 현재 노드가 코너라서 openList에서 나온경우 코너 방향으로 탐색 추가
		// 코너로 생성되는 방향이 최대 2개이기 때문에 BYTE를 2개 선언한것
		if (pCurrent->_forced_neighbour_info != is_not_dir)
		{
			BYTE dir_first = (Direction)get_lower_4bit(pCurrent->_forced_neighbour_info);
			BYTE dir_second = (Direction)get_upper_4bit(pCurrent->_forced_neighbour_info);
			if (is_dir_diagonal(dir_first))
				search_diagonal(pCurrent, pCurrent->_pos + Coordinate::_direction[dir_first], (Direction)dir_first, hWnd);
			if (is_dir_diagonal(dir_second))
				search_diagonal(pCurrent, pCurrent->_pos + Coordinate::_direction[dir_second], (Direction)dir_second, hWnd);
		}

		// 기존 방향으로의 탐색
		Coordinate next_vertex;
		if (is_linear((Direction)pCurrent->_direction) && search_linear(pCurrent,pCurrent->_pos + Coordinate::_direction[pCurrent->_direction], &next_vertex, (Direction)pCurrent->_direction, &forced_dir, hWnd))
		{
			g_Tile[next_vertex] = (char)CANDIDATE;
			openList_.insert(pair{ next_vertex, new VERTEX{ next_vertex, dest_, pCurrent,pCurrent->_direction,forced_dir} });
		}
		if (is_diagonal((Direction)pCurrent->_direction))
			search_diagonal(pCurrent, pCurrent->_pos, (Direction)pCurrent->_direction, hWnd);

#ifdef SHOW
		InvalidateRect(hWnd, NULL, FALSE);
		if (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif 
	}
	return false;
} 

// 성공시 *pForced_dir 에 방향삽입, 실패시 is_not_dir 삽입
bool JPS::isJumpPoint(Coordinate current ,const Direction search_direction,BYTE* pForced_dir)
{
	if (current == dest_)
	{
		*pForced_dir = is_not_dir;
		return true;
	}
	*pForced_dir = 0;
	bool is_success = false;
	std::pair<Direction, Direction> blocked_dir;
	std::pair<Direction, Direction> open_dir;
	determine_blocked_direction(search_direction, &blocked_dir);
	determine_open_direction(search_direction, &open_dir);

	Coordinate blocked = current + Coordinate::_direction[blocked_dir.first];
	Coordinate open = current + Coordinate::_direction[open_dir.first];
	bool is_blocked = !g_Tile.index_outOf_range(blocked) && g_Tile[blocked] == OBSTACLE;
	bool is_open = !g_Tile.index_outOf_range(open) && g_Tile[open] != OBSTACLE;
	if (is_blocked && is_open)
	{
		*pForced_dir |= open_dir.first;
		is_success = true;
	}
	blocked = current + Coordinate::_direction[blocked_dir.second];
	open = current + Coordinate::_direction[open_dir.second];
	is_blocked = !g_Tile.index_outOf_range(blocked) && g_Tile[blocked] == OBSTACLE;
	is_open =  !g_Tile.index_outOf_range(open) && g_Tile[open] != OBSTACLE;
	if(is_blocked && is_open)
	{
		*pForced_dir |= open_dir.second << 4;
		is_success = true;
	}
	if (!is_success)
		*pForced_dir = is_not_dir;
	return is_success;
}
// 탐색 결과가 searched 된곳은 전부 색칠됨, 만약 노드를 생성할 위치를 찾앗다면 그부분은 호출자가 직접 색칠해야함
// current_pos로 들어온 방향부터 
bool JPS::search_linear(VERTEX* parent,Coordinate current, Coordinate* pNext_vertex_coordinate, Direction direction, BYTE* pForced_dir, HWND hWnd)
{
	while (g_Tile.valid_pos(current))
	{
		// 현재 위치에 하나 이상의 forced_neighbor가 있으면 jump point
		bool is_start = g_Tile[current] == START;
		bool is_search_start_point_visited = g_Tile[parent->_pos] == VISITED;
		bool is_visited = g_Tile[current] == VISITED;
		bool is_candidate = g_Tile[current] == CANDIDATE;
		// 대각선의 경우 탐색을 맨 처음 시작한 부모노드와 보조탐색을 시작한 위치가 다를때 동일선상에 존재하는 노드끼리만 부모를 보정하기 위해서 만든 변수
		bool is_horizontal_or_vertical = current._x == parent->_pos._x || current._y == parent->_pos._y;

		// 이미 closelist에 존재하는 노드는 보정을 진행하지 않는다 (물론 하면 더 최단경로에 가까워질 가능성이 잇지만 성능상 JPS의 의미가 퇴색될것이다)
		if (!is_start && !is_visited && isJumpPoint(current, direction, pForced_dir))
		{
			// 현재 이미 코너로서 오픈리스트에 존재하는 노드지만 나의 기준에서도 코너인경우
			// 노드와 부모의 관계는 동일 수직 혹은 수평선상에 존재하는데 노드가 이미 다른노드의 자식인경우임 
			if ((is_candidate) && is_horizontal_or_vertical && is_search_start_point_visited)
			{
				// G값을 비교해서 더 작다면
				VERTEX* pExist = openList_.find(current)->second;
				double compare_G = getGHF::get_G(current, parent);
				if (compare_G < pExist->_G)
				{
					pExist->_parent_vertex = parent;
					pExist->_direction = (BYTE)direction;
					pExist->_forced_neighbour_info = *pForced_dir;
					pExist->_G = compare_G;
					pExist->_F = compare_G + pExist->_H;
					return false; // 노드는 새로 생성하면 안됨
				}
			}
			//코너지만 오픈리스트에는 없는 노드
			else if(!is_visited)
			{
				*pNext_vertex_coordinate = current;
				return true;
			}
		}
		// 수평선상에 존재하는 closelist에 존재하는 노드 기준에서는 코너가 아니지만 부모가 바뀌면 G값이 작은경우
		else if (is_candidate && is_horizontal_or_vertical)
		{
			VERTEX* pExist = openList_.find(current)->second;
			double compare_G = getGHF::get_G(current, parent);
			if (compare_G < pExist->_G)
			{
				pExist->_parent_vertex = parent;
				//pExist->_direction = (BYTE)direction;
				//pExist->_forced_neighbour_info = *pForced_dir;
				pExist->_G = compare_G;
				pExist->_F = compare_G + pExist->_H;
			}
		}
		if (g_Tile[current] != START && g_Tile[current] != CANDIDATE && g_Tile[current] != VISITED) g_Tile[current] = SEARCHED;
		current+= Coordinate::_direction[direction];
	}
#ifdef PRINT
	InvalidateRect(hWnd, NULL, FALSE);
	if (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
	// 탐색의 결과 jumppoint가 탐색되지 않고 장애물 혹은 벽을 만나 끝난경우
	pNext_vertex_coordinate = nullptr; 
	return false;
}

void JPS::search_diagonal(VERTEX* parent, Coordinate current, const Direction direction, HWND hWnd)
{
	// search_linear와는 다르게 while시작전 할게잇기떄문에 미리 체크
	if(!g_Tile.valid_pos(current))
		return; 
	Coordinate next_horizontal;
	Coordinate next_vertical;
	BYTE corner_dir;
	BYTE corner_dir_horizontal;
	BYTE corner_dir_vertical;
	std::pair<Direction, Direction> auxiliary_direction;
	dertermine_auxiliary_direction(direction, &auxiliary_direction);
	bool search_horizontal;
	bool search_vertical;
	bool is_candidate = g_Tile[current] == CANDIDATE;
	bool is_visited = g_Tile[current] == VISITED;
	//보조탐색으로 인한 결과로 이자리에 노드가 생성된뒤 대각선탐색함수를 해당위치에서 다시 호출햇을때 보조탐색으로 발견된 코너 위치에 vertex를 추가후 이동 
	if (parent->_pos == current)
	{
			search_vertical = search_linear(parent, current + Coordinate::_direction[auxiliary_direction.second], &next_vertical, auxiliary_direction.second, &corner_dir_vertical, hWnd);
			search_horizontal = search_linear(parent, current + Coordinate::_direction[auxiliary_direction.first], &next_horizontal, auxiliary_direction.first, &corner_dir_horizontal, hWnd);
			if (search_horizontal && g_Tile[next_horizontal] != VISITED && g_Tile[next_horizontal] != CANDIDATE)
			{
				openList_.insert(pair{ next_horizontal, new VERTEX{ next_horizontal, dest_, parent, (BYTE)auxiliary_direction.first, corner_dir_horizontal} });
				g_Tile[next_horizontal] = CANDIDATE;
			}
			if (search_vertical && g_Tile[next_vertical] != VISITED && g_Tile[next_vertical] != CANDIDATE)
			{
				openList_.insert(pair{ next_vertical, new VERTEX{ next_vertical, dest_, parent, (BYTE)auxiliary_direction.second, corner_dir_vertical} });
				g_Tile[next_vertical] = CANDIDATE;
			}
		current += Coordinate::_direction[direction];
	}

	while (g_Tile.valid_pos(current))
	{
		is_candidate = g_Tile[current] == CANDIDATE;
		is_visited = g_Tile[current] == VISITED;
		// 우선 그자리가 코너인지 체크하고 코너라면 vertex를 추가하고 끝냄
		if (!is_visited && isJumpPoint(current, direction, &corner_dir))
		{
			// 지금 기준으로도 코너인데 이미 누가 vertex를 만들어놓은경우 만약 G값이 더 작으면 vertex자체를 대체함
			if (is_candidate)
				//if (is_candidate || is_visited)
			{
				VERTEX* pExist = openList_.find(current)->second;
				double compare_G = getGHF::get_G(current, parent);
				if (compare_G < pExist->_G)
				{
					pExist->_G = compare_G;
					pExist->_parent_vertex = parent;
					pExist->_direction = (BYTE)direction;
					pExist->_forced_neighbour_info = corner_dir;
					pExist->_F = compare_G + pExist->_H;
				}
			}
			else if (!is_visited)
			{
				openList_.insert(pair{ current, new VERTEX{ current, dest_, parent, (BYTE)direction,corner_dir} });
				g_Tile[current] = CANDIDATE;
			}
			return;
		}
		// 대각선 탐색이면서 그 위치가 코너가 아니라면 보조탐색 결과로 인해서 코너가 생긴것이다 
		// is_candidate와 is_visited는 무조건 둘중 하나는 거짓이다 둘다 참이되는건 불가능
		// 지금 위치가 오픈리스트에 없는데 보조탐색을 통해 코너를 찾으면 현재 위치에 노드를 추가하고 끝냄

		if (!is_visited)
		{
			search_horizontal = search_linear(parent, current + Coordinate::_direction[auxiliary_direction.first], &next_horizontal, auxiliary_direction.first, &corner_dir_horizontal, hWnd);
			search_vertical = search_linear(parent, current + Coordinate::_direction[auxiliary_direction.second], &next_vertical, auxiliary_direction.second, &corner_dir_vertical, hWnd);
			if ((search_horizontal || search_vertical))
			{
				openList_.insert(pair{ current, new VERTEX{ current, dest_, parent, (BYTE)direction,is_not_dir} });
				g_Tile[current] = CANDIDATE;
				return;
			}
		}
			// 색칠
		if (g_Tile[current] != START && g_Tile[current] != CANDIDATE && g_Tile[current] != VISITED) g_Tile[current] = SEARCHED;

		current += Coordinate::_direction[direction];
	}


}
