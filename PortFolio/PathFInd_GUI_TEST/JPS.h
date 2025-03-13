#pragma once
#include <cmath>
#include "Tile.h"
extern Tile g_Tile;
class JPS : public PATHFINDER
{
public:
	typedef Coordinate::EightDirection Direction;
	JPS();
	bool pathFind(HWND hWnd) override;

private:
	// 여기서의 direction은 현재 위치에서 다음 위치로 이동하는 방향을 의미
	// search* 에서 이걸 성공하면 바로 그 위치에 노드를 추가하면됨
	bool isJumpPoint(Coordinate current_pos, const Direction search_direction,BYTE* pForced_dir);
	bool search_linear(VERTEX* parent, Coordinate current, Coordinate* pNext_vertex_coordinate, const Direction direction, BYTE* pForced_dir, HWND hWnd);
	void search_diagonal(VERTEX* parent, Coordinate current_pos, const Direction direction, HWND hWnd);

	__forceinline void determine_blocked_direction(const Direction search_direction, std::pair<Direction, Direction>* pBlocked_direction)
	{
		switch (search_direction)
		{
		case Coordinate::EAST:
		case Coordinate::WEST:
			*pBlocked_direction = { Coordinate::NORTH, Coordinate::SOUTH };
			break;
		case Coordinate::NORTH:
		case Coordinate::SOUTH:
			*pBlocked_direction = { Coordinate::EAST, Coordinate::WEST };
			break;
		case Coordinate::NORTH_EAST:
			*pBlocked_direction = { Coordinate::WEST, Coordinate::SOUTH };
			break;
		case Coordinate::NORTH_WEST:
			*pBlocked_direction = { Coordinate::EAST, Coordinate::SOUTH };
			break;
		case Coordinate::SOUTH_EAST:
			*pBlocked_direction = { Coordinate::WEST, Coordinate::NORTH };
			break;
		case Coordinate::SOUTH_WEST:
			*pBlocked_direction = { Coordinate::EAST, Coordinate::NORTH };
			break;
		default:
			break;
		}
	}
	__forceinline void determine_open_direction(const Direction search_direction, std::pair<Direction, Direction>* pOpen_direction)
	{
		switch (search_direction)
		{
		case Coordinate::EAST:
			*pOpen_direction = { Coordinate::NORTH_EAST, Coordinate::SOUTH_EAST };
			break;
		case Coordinate::WEST:
			*pOpen_direction = { Coordinate::NORTH_WEST, Coordinate::SOUTH_WEST };
			break;
		case Coordinate::NORTH:
			*pOpen_direction = { Coordinate::NORTH_EAST, Coordinate::NORTH_WEST };
			break;
		case Coordinate::SOUTH:
			*pOpen_direction = { Coordinate::SOUTH_EAST, Coordinate::SOUTH_WEST };
			break;
		case Coordinate::NORTH_EAST:
			*pOpen_direction = { Coordinate::NORTH_WEST, Coordinate::SOUTH_EAST };
			break;
		case Coordinate::NORTH_WEST:
			*pOpen_direction = { Coordinate::NORTH_EAST, Coordinate::SOUTH_WEST };
			break;
		case Coordinate::SOUTH_EAST:
			*pOpen_direction = { Coordinate::SOUTH_WEST, Coordinate::NORTH_EAST };
			break;
		case Coordinate::SOUTH_WEST:
			*pOpen_direction = { Coordinate::SOUTH_EAST, Coordinate::NORTH_WEST };
			break;
		default:
			break;
		}
	}
	// 대각선 방향 노드의 보조탐색의 방향을 결정하는 함수 , first는 수평, second는 수직
	__forceinline void dertermine_auxiliary_direction(const Direction search_direction, std::pair<Direction, Direction>* pAuxiliary_direction)
	{
		switch (search_direction)
		{
		case Coordinate::NORTH_EAST:
			*pAuxiliary_direction = { Coordinate::EAST, Coordinate::NORTH };
			break;
		case Coordinate::NORTH_WEST:
			*pAuxiliary_direction = { Coordinate::WEST, Coordinate::NORTH };
			break;
		case Coordinate::SOUTH_WEST:
			*pAuxiliary_direction = { Coordinate::WEST, Coordinate::SOUTH };
			break;
		case Coordinate::SOUTH_EAST:
			*pAuxiliary_direction = { Coordinate::EAST, Coordinate::SOUTH };
			break;
		default:
			break;
		}
	}
	__forceinline BYTE get_lower_4bit(BYTE forced__neighbor_search_result) { return forced__neighbor_search_result & 0b00001111; }
	__forceinline BYTE get_upper_4bit(BYTE forced__neighbor_search_result) { return (forced__neighbor_search_result & 0b11110000) >> 4; }
	__forceinline bool is_dir_diagonal(BYTE dir) { return Coordinate::NORTH_EAST == dir || Coordinate::NORTH_WEST == dir || Coordinate::SOUTH_EAST == dir || Coordinate::SOUTH_WEST == dir; }
	__forceinline bool is_linear(Direction direction) { return direction % 2 == 0 && direction < Direction::TOTAL; }
	__forceinline bool is_diagonal(Direction direction) { return direction % 2 == 1 && direction < Direction::TOTAL; }
};


