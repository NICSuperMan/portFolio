#pragma once
struct pointCompare {
	bool operator()(const Coordinate& lhs, const Coordinate& rhs) const;
};

class PATHFINDER
{
public:
	bool isFindPathSuccess_ = false;
	static bool isSetStartPoint_;
	static bool isSetDest;
	static Coordinate dest_;
	static Coordinate start_;
	static std::map<Coordinate, VERTEX*, pointCompare>openList_;
	static std::map<Coordinate,VERTEX*,pointCompare> closeList_;
	static VERTEX* pCurrentVertex_;

	PATHFINDER(bool is_find_path_success = false);
	void test(HWND hWnd);
	void clearList_by_selection(bool is_clear_open_list, bool is_clear_close_list);
	virtual bool pathFind(HWND hWnd) = 0;
	static bool _isStart(void);
	void reset();
	static void drawPolyLine(HDC hdc);
};
