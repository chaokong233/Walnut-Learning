#pragma once

#include <random>
#define _USE_MATH_DEFINES
#include "math.h"
#include <cmath>
#include <algorithm>

#include <glm/glm.hpp>

namespace Walnut {

	class Random
	{
	public:
		static void Init()
		{
			s_RandomEngine.seed(std::random_device()());
		}

		static uint32_t UInt()
		{
			return s_Distribution(s_RandomEngine);
		}

		static uint32_t UInt(uint32_t min, uint32_t max)
		{
			return min + (s_Distribution(s_RandomEngine) % (max - min + 1));
		}

		static float Float()
		{
			return (float)s_Distribution(s_RandomEngine) / (float)std::numeric_limits<uint32_t>::max();
		}

		static float Float(float min, float max)
		{
			return Float() * (max - min) + min;
		}

		static glm::vec3 Vec3()
		{
			return glm::vec3(Float(), Float(), Float());
		}

		static glm::vec3 Vec3(float min, float max)
		{
			return glm::vec3(Float() * (max - min) + min, Float() * (max - min) + min, Float() * (max - min) + min);
		}

		static glm::vec3 InUnitSphere()
		{
			auto a = Float(0, 2 * M_PI);
			auto z = Float(-1, 1);
			auto r = sqrt(1 - z*z);
			return glm::vec3(r*cos(a), r*sin(a), z);
		}

		static glm::vec2 InUnitCircle()
		{
			auto theta = Float(0, 2 * M_PI);
			auto r = Float(0.0f, 1.0f);
			return glm::vec2(r*cos(theta), r*sin(theta));
		}

	private:
		static std::random_device rd;
		static std::mt19937 s_RandomEngine;
		static std::uniform_int_distribution<std::mt19937::result_type> s_Distribution;
	};

}


