#include "Ray.h"



uint32_t uint_Power(uint32_t src, float para)
{
	uint32_t res = 0xff000000;
	for (int i = 0; i < 3; i++)
	{
		int bit = i * 8;
		res |= static_cast<uint32_t>(std::pow((float)(((src >> bit) & 0x000000ff) / (float)0x000000ff), para) * 0x000000ff) << bit;
	}
	return res;
}


