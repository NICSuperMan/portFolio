#include <cassert>
#include <cmath>
#include <Windows.h>
#include "VERTEX.h"
#include "Coordinate.h"
#include "GHF.h"
double getGHF::get_Euclidean_distance_from_parent(const Coordinate& current_position, const VERTEX* parentVertex)
{
    return sqrt(pow(parentVertex->_pos._x - current_position._x, 2) + pow(parentVertex->_pos._y - current_position._y, 2));
}
double getGHF::get_G(const Coordinate& current_position, const VERTEX* parentVertex)
{ 
  if (parentVertex == nullptr)
        return 0.0;

  return getGHF::get_Euclidean_distance_from_parent(current_position, parentVertex) + parentVertex->_G;
}
int getGHF::get_Huristic_manhatan(const Coordinate& current_position, const Coordinate& destination)
{
    return std::abs(destination._x - current_position._x) + std::abs(destination._y - current_position._y);
}

