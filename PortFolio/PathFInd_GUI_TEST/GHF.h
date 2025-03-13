#pragma once
namespace getGHF
{
	double get_Euclidean_distance_from_parent(const Coordinate& current_position, const VERTEX* parentVertex);
	double get_G(const Coordinate& current_position, const VERTEX* parentVertex);
	int get_Huristic_manhatan(const Coordinate& current_position, const Coordinate& destination);
}

