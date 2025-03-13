#pragma once
#include "PATHFINDER.h"


class ASTAR : public PATHFINDER
{
public:
	bool pathFind(HWND hWnd);
	void drawParentLine(HDC hdc);
	ASTAR();
};


