#pragma once
#include <cassert>
#include <Windows.h>
#include "Coordinate.h"
enum STATE { NOSTATE, OBSTACLE, START, DESTINATION, CANDIDATE, VISITED, SEARCHED };

class Tile
{
public:
	Tile();
	static constexpr int GRID_HEIGHT = 101; 
	static constexpr int GRID_WIDTH = 191;
	static constexpr int _GRID_SIZE = 10;
	char& operator[](const Coordinate& index);
	const char operator[](const Coordinate& index) const;
	bool index_outOf_range(const Coordinate& index);
	bool valid_pos(const Coordinate index);
	void initialize_tile();
	void make_map();
	void make_map_render(HWND hWnd);
	void polling(HWND hWnd);
	void reappearnce_all_1s_tile(HWND hWnd);
	void clear_tile(bool isClear_start_dest, bool isClear_open_list, bool isClear_close_list, bool isClear_obstacle,bool isClear_search);
	__forceinline int count_obs()
	{
		int obs_num = 0;
		for (int y = 0; y < GRID_WIDTH; ++y)
		{
			for (int x = 0; x < GRID_HEIGHT; ++x)
			{
				if (_tile[y][x] == OBSTACLE)
					++obs_num;
			}
		}
		return obs_num;
	}

	char _tile[GRID_HEIGHT][GRID_WIDTH];

	// 디버깅용
	void backup_map_and_setting();
	void set_map_and_setting(HWND hWnd);
private:
	friend Coordinate* make_BFS_start_pos(const Tile& tile);
	// 막힌지점 없애는 함수
	friend bool remove_Dead_end(Tile& tile);
	friend bool remove_Dead_end_render(Tile& tile, HWND hWnd);
	

	
};
