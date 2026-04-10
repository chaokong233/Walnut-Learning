#pragma once

#include <cmath>
#include <algorithm>

const double infinity = std::numeric_limits<double>::infinity();
constexpr bool isDenoseUsePrefilter = true;
const double PI = 3.1415926535897;

float angle_to_radius(float angle)
{
	return angle * 180.0f / PI;
}
int sign(float v)
{
	return v > 0 ? 1 : -1;
}
