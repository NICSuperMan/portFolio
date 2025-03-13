#pragma once
#include "Coordinate.h"
#include "GHF.h"
struct VERTEX 
{
	Coordinate _pos;
	const VERTEX* _parent_vertex;
	double _G;
	int _H;
	double _F;

	// JPS에서만 쓰는 멤버들입니다.
	BYTE _direction;
	BYTE _forced_neighbour_info;
	
	bool operator<(const VERTEX& other) const {
		return _F < other._F;
	}
	VERTEX() = default;
	VERTEX(const Coordinate& pos, const Coordinate& destination, const VERTEX* parentVertex = nullptr, BYTE direction = 0b11111111, BYTE forced_neighbour_info = 0b11111111)
		:_pos{ pos }, _parent_vertex{ parentVertex }, _G{ getGHF::get_G(pos,parentVertex) }, _H{ getGHF::get_Huristic_manhatan(pos,destination) }, _direction{ direction }, _forced_neighbour_info{ forced_neighbour_info }
	{
		_F = _G + _H;
	}
};

struct cmp
{
	bool operator()(const VERTEX& first, const VERTEX& second)
	{
		return first._F > second._F;
	}
};



