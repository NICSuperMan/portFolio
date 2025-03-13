#pragma once
struct VERTEX;
class Coordinate
{
public:
	int _x;
	int _y;
	Coordinate(const int x, const int y)
	 :_x{ x }, _y{ y } {}	
	Coordinate()
		:_x{ 0 }, _y{ 0 } {};

	_forceinline Coordinate(const Coordinate& other);

	friend Coordinate operator+(const Coordinate& left_operand, const Coordinate& right_operand)
	{
		return Coordinate{ left_operand._x + right_operand._x, left_operand._y + right_operand._y };
	}
	friend Coordinate operator-(const Coordinate& left_operand, const Coordinate& right_operand);	
	friend Coordinate operator-(const Coordinate& left_operand, const int right_operand);
	friend Coordinate operator-(Coordinate&& left_operand, Coordinate&& right_operand);
	friend Coordinate&& operator+(Coordinate&& left_operand, const int right_operand);
	friend Coordinate&& operator+(Coordinate&& left_operand, const Coordinate& right_operand);
	Coordinate&& operator-(const int right_operand);
	friend Coordinate operator*(const Coordinate& left_operand, const int right_operand);
	friend bool operator==(const Coordinate& left_operand, const Coordinate& right_operand);
	friend bool operator!=(const Coordinate& left_operand, const Coordinate& right_operand);
	friend Coordinate operator/(const Coordinate& left_operand, int right_operand)
	{
		if (right_operand == 0)
			assert(right_operand != 0 && "Divide by zero");
		return Coordinate{ left_operand._x / right_operand,left_operand._y / right_operand };
	}
	friend Coordinate&& operator/(Coordinate&& left_operand, int right_operand);
	Coordinate& operator=(const Coordinate& right_operand);
	Coordinate& operator+=(const Coordinate& other);
	Coordinate& operator/=(const int& right_operand);	
	enum EightDirection : unsigned
	{
		NORTH,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		NORTH_WEST,
		TOTAL
	};
	static Coordinate _direction[TOTAL];
};
Coordinate make_midPoint(const VERTEX* current_vertex);
Coordinate make_mid_Pixel(const Coordinate& target);


