#include "pch.h"

#include "FLRandom.h"


int32 FLRandom::GetFromRange(int32 minInclusive, int32 maxInclusive)
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int32> dist(minInclusive, maxInclusive);

	return dist(mt);
}
