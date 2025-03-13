#include <Windows.h>
#include <cassert>
#include <utility>
#include "VERTEX.h"
#include <cassert>
#include "Coordinate.h"
#include "Tile.h"
#pragma warning (disable :  4244)
extern int g_scroll_up;
extern Coordinate g_render_start_point;

Coordinate operator-(const Coordinate& left_operand, const Coordinate& right_operand)
{
    return Coordinate{ left_operand._x - right_operand._x,left_operand._y - right_operand._y };
}

Coordinate operator-(const Coordinate& left_operand, const int right_operand)
{
    return Coordinate{ left_operand._x - right_operand,left_operand._y - right_operand };
}

Coordinate operator-(Coordinate&& left_operand, Coordinate&& right_operand)
{
    left_operand._x -= right_operand._x;
    left_operand._y -= right_operand._y;
    return std::move(left_operand); 
}

Coordinate&& operator+(Coordinate&& left_operand, const int right_operand)
{
    left_operand._x += right_operand;
    left_operand._y += right_operand;
    return std::move(left_operand);
}

Coordinate&& operator+(Coordinate&& left_operand, const Coordinate& right_operand)
{
    left_operand._x += right_operand._x;
    left_operand._y += right_operand._y;
    return std::move(left_operand);
}

Coordinate operator*(const Coordinate& left_operand, const int right_operand)
{
    return Coordinate{ left_operand._x * right_operand,left_operand._y * right_operand };
}

bool operator==(const Coordinate& left_operand, const Coordinate& right_operand)
{
    return left_operand._x == right_operand._x && left_operand._y == right_operand._y;
}

bool operator!=(const Coordinate& left_operand, const Coordinate& right_operand)
{
    return !operator==(left_operand, right_operand);
}

Coordinate&& operator/(Coordinate&& left_operand, int right_operand)
{
    if (right_operand == 0)
        assert(right_operand != 0 && "Divide by zero");

    left_operand._x /= right_operand;
    left_operand._y /= right_operand;
    return std::move(left_operand);
}

_forceinline Coordinate::Coordinate(const Coordinate& other)
	: _x{ other._x }, _y{ other._y } {}

Coordinate&& Coordinate::operator-(const int right_operand)
{
    _x -= right_operand;
    _y -= right_operand;   
    return std::move(*this);
}


Coordinate& Coordinate::operator=(const Coordinate& right_operand)
{
    if (this != &right_operand)
    {
		_x = right_operand._x;
		_y = right_operand._y;
    }
		return *this;
}

Coordinate& Coordinate::operator+=(const Coordinate& other)
{
    _x += other._x;
    _y += other._y;
    return *this;
}

Coordinate& Coordinate::operator/=(const int& right_operand)
{
    assert(right_operand != 0 && "Divide by zero");
    _x /= right_operand;
    _y /= right_operand;
    return *this;
}

Coordinate Coordinate::_direction[TOTAL] = {
	Coordinate{0,-1},
	Coordinate{1,-1},
	Coordinate{1,0},
	Coordinate{1,1},
	Coordinate{0,1},
	Coordinate{-1,1},
	Coordinate{-1,0},
	Coordinate{-1,-1}
};

Coordinate make_midPoint(const VERTEX* current_vertex)
{
	// 여기 null이 들어오면 진짜 말이 안되는 심각한 상황
	if (!current_vertex)
		throw;
	int extended_grid_size = Tile::_GRID_SIZE + g_scroll_up;
    Coordinate&& render_pixel = (current_vertex->_pos - g_render_start_point) * extended_grid_size;
	int x = (2 * render_pixel._x + extended_grid_size) / 2.0;
	int y = (2 * render_pixel._y + extended_grid_size) / 2.0;
	return Coordinate{ x,y };
}
Coordinate make_mid_Pixel(const Coordinate& target)
{
    int extended_grid_size = Tile::_GRID_SIZE + g_scroll_up;
    Coordinate render_pixel = (target - g_render_start_point) * extended_grid_size;
    int x = (2 * render_pixel._x + extended_grid_size) / 2.0;
    int y = (2 * render_pixel._y + extended_grid_size) / 2.0;
    return Coordinate{ x,y };
}

