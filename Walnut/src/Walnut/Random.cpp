#include "Random.h"

namespace Walnut {
	std::random_device Random::rd;
	std::mt19937 Random::s_RandomEngine(rd());
	std::uniform_int_distribution<std::mt19937::result_type> Random::s_Distribution;

}